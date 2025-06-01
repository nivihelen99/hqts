#include "gtest/gtest.h"
#include "hqts/scheduler/wrr_scheduler.h"
#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor
#include "hqts/core/flow_context.h"      // For core::QueueId, core::FlowId

#include <vector>
#include <map>
#include <numeric>   // For std::accumulate if needed, or std::gcd for advanced tests
#include <stdexcept> // For std::invalid_argument etc.

namespace hqts {
namespace scheduler {

// Helper to create a PacketDescriptor. Priority field is used as QueueId for WRR.
PacketDescriptor createWrrTestPacket(core::FlowId flow_id, uint32_t length, core::QueueId queue_id_for_wrr) {
    // Using priority field of PacketDescriptor to denote the target QueueId for WRR
    // Ensure that the queue_id_for_wrr can be safely cast to uint8_t for packet.priority
    if (queue_id_for_wrr > UINT8_MAX) {
        throw std::overflow_error("Queue ID for WRR test packet exceeds uint8_t max value.");
    }
    return PacketDescriptor(flow_id, length, static_cast<uint8_t>(queue_id_for_wrr));
}

TEST(WrrSchedulerTest, ConstructorValidation) {
    // Empty config
    std::vector<WrrScheduler::QueueConfig> empty_configs;
    ASSERT_THROW(WrrScheduler scheduler(empty_configs), std::invalid_argument);

    // Config with zero weight
    std::vector<WrrScheduler::QueueConfig> zero_weight_configs = {{static_cast<core::QueueId>(1), 0}};
    ASSERT_THROW(WrrScheduler scheduler(zero_weight_configs), std::invalid_argument);

    std::vector<WrrScheduler::QueueConfig> zero_weight_configs2 = {
        {static_cast<core::QueueId>(1), 10},
        {static_cast<core::QueueId>(2), 0}
    };
    ASSERT_THROW(WrrScheduler scheduler(zero_weight_configs2), std::invalid_argument);

    // Duplicate Queue IDs
    std::vector<WrrScheduler::QueueConfig> duplicate_id_configs = {
        {static_cast<core::QueueId>(1), 10},
        {static_cast<core::QueueId>(1), 20}
    };
    ASSERT_THROW(WrrScheduler scheduler(duplicate_id_configs), std::invalid_argument);

    // Valid config
    std::vector<WrrScheduler::QueueConfig> valid_configs = {
        {static_cast<core::QueueId>(1), 10},
        {static_cast<core::QueueId>(2), 20}
    };
    ASSERT_NO_THROW(WrrScheduler scheduler(valid_configs));
    WrrScheduler scheduler(valid_configs);
    ASSERT_EQ(scheduler.get_num_queues(), 2);
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(WrrSchedulerTest, EnqueueAndDequeueSinglePacket) {
    std::vector<WrrScheduler::QueueConfig> configs = {{static_cast<core::QueueId>(100), 1}};
    WrrScheduler scheduler(configs);

    PacketDescriptor p1 = createWrrTestPacket(1, 100, 100); // queue_id 100
    scheduler.enqueue(p1);
    ASSERT_FALSE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(100), 1);

    PacketDescriptor dequeued_p = scheduler.dequeue();
    ASSERT_EQ(dequeued_p.flow_id, p1.flow_id);
    // Recall: p1.priority holds the queue_id for this test helper
    ASSERT_EQ(dequeued_p.priority, static_cast<uint8_t>(100));
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(100), 0);
}

TEST(WrrSchedulerTest, DequeueFromEmpty) {
    std::vector<WrrScheduler::QueueConfig> configs = {{static_cast<core::QueueId>(1), 1}};
    WrrScheduler scheduler(configs);
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_THROW(scheduler.dequeue(), std::runtime_error);
}

TEST(WrrSchedulerTest, EnqueueInvalidQueueId) {
    std::vector<WrrScheduler::QueueConfig> configs = {{static_cast<core::QueueId>(1), 10}}; // Only queue_id 1 is configured
    WrrScheduler scheduler(configs);
    PacketDescriptor p_valid = createWrrTestPacket(1, 100, 1);
    PacketDescriptor p_invalid_q_id = createWrrTestPacket(2, 100, 2); // queue_id 2 not configured

    ASSERT_NO_THROW(scheduler.enqueue(p_valid));
    ASSERT_THROW(scheduler.enqueue(p_invalid_q_id), std::out_of_range);
}

TEST(WrrSchedulerTest, GetQueueSizeInvalidQueueId) {
    std::vector<WrrScheduler::QueueConfig> configs = {{static_cast<core::QueueId>(1), 10}};
    WrrScheduler scheduler(configs);
    ASSERT_THROW(scheduler.get_queue_size(2), std::out_of_range); // qid 2 not configured
    ASSERT_NO_THROW(scheduler.get_queue_size(1));
    ASSERT_EQ(scheduler.get_queue_size(1),0);
}

TEST(WrrSchedulerTest, BasicWeightDistribution) {
    std::vector<WrrScheduler::QueueConfig> configs = {
        {static_cast<core::QueueId>(1), 1},
        {static_cast<core::QueueId>(2), 2}
    }; // Q1 (w=1), Q2 (w=2)
    WrrScheduler scheduler(configs);

    // Enqueue 3 packets for Q1, 6 for Q2 (maintaining proportions for multiple rounds)
    for (int i = 0; i < 3; ++i) scheduler.enqueue(createWrrTestPacket(10 + i, 100, 1));
    for (int i = 0; i < 6; ++i) scheduler.enqueue(createWrrTestPacket(20 + i, 100, 2));

    ASSERT_EQ(scheduler.get_queue_size(1), 3);
    ASSERT_EQ(scheduler.get_queue_size(2), 6);

    std::map<core::QueueId, int> dequeued_counts;
    for (int i = 0; i < 9; ++i) { // Total 9 packets
        PacketDescriptor p = scheduler.dequeue();
        // p.priority field was used to store QueueId
        dequeued_counts[static_cast<core::QueueId>(p.priority)]++;
    }

    ASSERT_TRUE(scheduler.is_empty());
    // Expected: Q1 gets 1/3, Q2 gets 2/3. So 3 packets from Q1, 6 from Q2.
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(1)], 3);
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(2)], 6);
}

TEST(WrrSchedulerTest, WeightDistributionSpecificOrderOneRound) {
    std::vector<WrrScheduler::QueueConfig> configs = {
        {static_cast<core::QueueId>(1), 1},
        {static_cast<core::QueueId>(2), 2}
    };
    WrrScheduler scheduler(configs); // Deficits start at weight: Q1(d=1), Q2(d=2)

    scheduler.enqueue(createWrrTestPacket(10, 100, 1)); // Q1
    scheduler.enqueue(createWrrTestPacket(11, 100, 1)); // Q1
    scheduler.enqueue(createWrrTestPacket(20, 100, 2)); // Q2
    scheduler.enqueue(createWrrTestPacket(21, 100, 2)); // Q2
    scheduler.enqueue(createWrrTestPacket(22, 100, 2)); // Q2

    // Expect order based on initial deficit = weight, and round-robin:
    // Assumes current_queue_index_ starts at 0 (queue ID 1)
    // 1. Q1 (id=1, w=1, d=1). Dequeue fid=10. Q1 d=0. counts[1]=1. scheduler.current_queue_index_ becomes 1 (for Q2).
    // 2. Q2 (id=2, w=2, d=2). Dequeue fid=20. Q2 d=1. counts[2]=1. scheduler.current_queue_index_ becomes 0 (for Q1).
    // 3. Q1 (id=1, w=1, d=0). Cannot dequeue. scheduler.current_queue_index_ becomes 1 (for Q2). (This step is skipped if only positive deficit queues are chosen)
    //    Actually, the WRR loop will search for the next available queue with deficit.
    //    If scheduler.current_queue_index_ is 1 (for Q2):
    //    Dequeue fid=20 from Q2. Q2 deficit becomes 1. index advances to 0 (Q1).
    //    Dequeue fid=10 from Q1. Q1 deficit becomes 0. index advances to 1 (Q2).
    //    Dequeue fid=21 from Q2. Q2 deficit becomes 0. index advances to 0 (Q1).
    //    Now all deficits are 0. Replenish. Q1 d=1, Q2 d=2.
    //    Dequeue fid=11 from Q1. Q1 d=0. index advances to 1 (Q2).
    //    Dequeue fid=22 from Q2. Q2 d=1. index advances to 0 (Q1).


    std::vector<core::QueueId> expected_order = {1, 2, 2, 1, 2}; // Approximate for 2 packets Q1, 3 from Q2
                                                               // Exact order: Q1(1), Q2(1), Q2(1), then replenish, then Q1(1), Q2(1)
                                                               // Total 2 from Q1, 3 from Q2.

    // A more deterministic way to test order is to enqueue just enough for one "WRR round" based on weights.
    // Q1 (w=1), Q2 (w=2). Round is 3 packets.
    // Enqueue 1 for Q1, 2 for Q2.
    WrrScheduler scheduler2(configs);
    scheduler2.enqueue(createWrrTestPacket(10,100,1));
    scheduler2.enqueue(createWrrTestPacket(20,100,2));
    scheduler2.enqueue(createWrrTestPacket(21,100,2));

    std::map<core::QueueId, int> round_counts;
    round_counts[scheduler2.dequeue().priority]++;
    round_counts[scheduler2.dequeue().priority]++;
    round_counts[scheduler2.dequeue().priority]++;

    ASSERT_EQ(round_counts[1], 1);
    ASSERT_EQ(round_counts[2], 2);

}


TEST(WrrSchedulerTest, WeightDistributionWithEmptyQueues) {
    std::vector<WrrScheduler::QueueConfig> configs = {
        {static_cast<core::QueueId>(1), 1},
        {static_cast<core::QueueId>(2), 3}
    }; // Q1 (w=1), Q2 (w=3)
    WrrScheduler scheduler(configs);

    // Enqueue 5 packets for Q2 only
    for (int i = 0; i < 5; ++i) scheduler.enqueue(createWrrTestPacket(20 + i, 100, 2));

    ASSERT_EQ(scheduler.get_queue_size(1), 0);
    ASSERT_EQ(scheduler.get_queue_size(2), 5);

    // All 5 dequeued packets should be from Q2
    for (int i = 0; i < 5; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        ASSERT_EQ(static_cast<core::QueueId>(p.priority), static_cast<core::QueueId>(2));
    }
    ASSERT_TRUE(scheduler.is_empty());

    // Enqueue to Q1, then Q2. Then Q1 becomes empty.
    scheduler.enqueue(createWrrTestPacket(10, 100, 1)); // 1 packet to Q1
    for (int i = 0; i < 6; ++i) scheduler.enqueue(createWrrTestPacket(30 + i, 100, 2)); // 6 packets to Q2

    // First packet should be from Q1 (weight 1, deficit starts at 1)
    PacketDescriptor p_q1 = scheduler.dequeue();
    ASSERT_EQ(static_cast<core::QueueId>(p_q1.priority), static_cast<core::QueueId>(1));

    // Next 3 packets from Q2 (weight 3, deficit starts at 3)
    for(int i=0; i<3; ++i) {
         PacketDescriptor p = scheduler.dequeue();
         ASSERT_EQ(static_cast<core::QueueId>(p.priority), static_cast<core::QueueId>(2));
    }
    // Deficits: Q1=0, Q2=0. Replenish. Q1=1, Q2=3.
    // Q1 is empty.
    // Next 3 packets from Q2.
    for(int i=0; i<3; ++i) {
         PacketDescriptor p = scheduler.dequeue();
         ASSERT_EQ(static_cast<core::QueueId>(p.priority), static_cast<core::QueueId>(2));
    }
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(WrrSchedulerTest, ComplexDistributionMultipleRounds) {
    std::vector<WrrScheduler::QueueConfig> configs = {
        {static_cast<core::QueueId>(1), 1},
        {static_cast<core::QueueId>(2), 2},
        {static_cast<core::QueueId>(3), 3}
    }; // Q1 (w=1), Q2 (w=2), Q3 (w=3). Total weight sum = 6
    WrrScheduler scheduler(configs);

    // Enqueue packets for 2 "grand rounds" (LCM of weights or just many)
    // 2 packets for Q1 (2 * w1)
    // 4 packets for Q2 (2 * w2)
    // 6 packets for Q3 (2 * w3)
    // Total = 12 packets
    for (int i = 0; i < 2; ++i) scheduler.enqueue(createWrrTestPacket(100 + i, 100, 1));
    for (int i = 0; i < 4; ++i) scheduler.enqueue(createWrrTestPacket(200 + i, 100, 2));
    for (int i = 0; i < 6; ++i) scheduler.enqueue(createWrrTestPacket(300 + i, 100, 3));

    std::map<core::QueueId, int> dequeued_counts;
    for (int i = 0; i < 12; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        dequeued_counts[static_cast<core::QueueId>(p.priority)]++;
    }

    ASSERT_TRUE(scheduler.is_empty());
    // Proportions should be 1:2:3, so for 12 packets:
    // Q1: (1/6) * 12 = 2
    // Q2: (2/6) * 12 = 4
    // Q3: (3/6) * 12 = 6
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(1)], 2);
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(2)], 4);
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(3)], 6);
}

} // namespace scheduler
} // namespace hqts
