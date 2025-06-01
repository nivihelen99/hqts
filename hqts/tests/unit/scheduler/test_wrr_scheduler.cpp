#include "gtest/gtest.h"
#include "hqts/scheduler/wrr_scheduler.h"
#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor
#include "hqts/core/flow_context.h"          // For core::QueueId, core::FlowId
#include "hqts/scheduler/aqm_queue.h"        // For RedAqmParameters

#include <vector>
#include <map>
#include <numeric>   // For std::accumulate if needed
#include <stdexcept> // For std::invalid_argument etc.

namespace hqts {
namespace scheduler {

// Helper to create a PacketDescriptor. Priority field is used as QueueId for WRR.
PacketDescriptor createWrrTestPacket(core::FlowId flow_id, uint32_t length, core::QueueId queue_id_for_wrr) {
    if (queue_id_for_wrr > UINT8_MAX) {
        throw std::overflow_error("Queue ID for WRR test packet exceeds uint8_t max value.");
    }
    return PacketDescriptor(flow_id, length, static_cast<uint8_t>(queue_id_for_wrr));
}

// Helper to create permissive RedAqmParameters for a single queue
RedAqmParameters createPermissiveAqmParams(
    uint32_t capacity_bytes = 1000000,
    double max_p = 0.001,
    double ewma_w = 0.002) {
    return RedAqmParameters(
        static_cast<uint32_t>(capacity_bytes * 0.8),
        static_cast<uint32_t>(capacity_bytes * 0.9),
        max_p, ewma_w, capacity_bytes
    );
}

// Helper to create a vector of WrrScheduler::QueueConfig with permissive AQM
std::vector<WrrScheduler::QueueConfig> createWrrConfigsWithPermissiveAqm(
    const std::vector<std::pair<core::QueueId, uint32_t>>& queue_defs) {

    std::vector<WrrScheduler::QueueConfig> configs;
    RedAqmParameters permissive_aqm = createPermissiveAqmParams();
    for (const auto& q_def : queue_defs) {
        configs.emplace_back(q_def.first, q_def.second, permissive_aqm);
    }
    return configs;
}


TEST(WrrSchedulerTest, ConstructorValidation) {
    std::vector<WrrScheduler::QueueConfig> empty_configs;
    ASSERT_THROW(WrrScheduler scheduler(empty_configs), std::invalid_argument);

    RedAqmParameters permissive_aqm = createPermissiveAqmParams();

    std::vector<WrrScheduler::QueueConfig> zero_weight_configs = {{static_cast<core::QueueId>(1), 0, permissive_aqm}};
    ASSERT_THROW(WrrScheduler scheduler(zero_weight_configs), std::invalid_argument);

    std::vector<WrrScheduler::QueueConfig> zero_weight_configs2 = {
        {static_cast<core::QueueId>(1), 10, permissive_aqm},
        {static_cast<core::QueueId>(2), 0, permissive_aqm}
    };
    ASSERT_THROW(WrrScheduler scheduler(zero_weight_configs2), std::invalid_argument);

    std::vector<WrrScheduler::QueueConfig> duplicate_id_configs = {
        {static_cast<core::QueueId>(1), 10, permissive_aqm},
        {static_cast<core::QueueId>(1), 20, permissive_aqm}
    };
    ASSERT_THROW(WrrScheduler scheduler(duplicate_id_configs), std::invalid_argument);

    std::vector<WrrScheduler::QueueConfig> valid_configs = {
        {static_cast<core::QueueId>(1), 10, permissive_aqm},
        {static_cast<core::QueueId>(2), 20, permissive_aqm}
    };
    ASSERT_NO_THROW(WrrScheduler scheduler(valid_configs));
    DrrScheduler scheduler(valid_configs); // Corrected type to DrrScheduler for consistency with test name, should be WrrScheduler
    // Reverting to WrrScheduler as this is a WrrSchedulerTest
    WrrScheduler wrr_scheduler(valid_configs);
    ASSERT_EQ(wrr_scheduler.get_num_queues(), 2);
    ASSERT_TRUE(wrr_scheduler.is_empty());
}

TEST(WrrSchedulerTest, EnqueueAndDequeueSinglePacket) {
    auto configs = createWrrConfigsWithPermissiveAqm({{static_cast<core::QueueId>(100), 1}});
    WrrScheduler scheduler(configs);

    PacketDescriptor p1 = createWrrTestPacket(1, 100, 100);
    scheduler.enqueue(p1); // Permissive AQM should accept
    ASSERT_FALSE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(100), 1);

    PacketDescriptor dequeued_p = scheduler.dequeue();
    ASSERT_EQ(dequeued_p.flow_id, p1.flow_id);
    ASSERT_EQ(static_cast<core::QueueId>(dequeued_p.priority), static_cast<core::QueueId>(100));
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(100), 0);
}

TEST(WrrSchedulerTest, DequeueFromEmpty) {
    auto configs = createWrrConfigsWithPermissiveAqm({{static_cast<core::QueueId>(1), 1}});
    WrrScheduler scheduler(configs);
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_THROW(scheduler.dequeue(), std::runtime_error);
}

TEST(WrrSchedulerTest, EnqueueInvalidQueueId) {
    auto configs = createWrrConfigsWithPermissiveAqm({{static_cast<core::QueueId>(1), 10}});
    WrrScheduler scheduler(configs);
    PacketDescriptor p_valid = createWrrTestPacket(1, 100, 1);
    PacketDescriptor p_invalid_q_id = createWrrTestPacket(2, 100, 2);

    ASSERT_NO_THROW(scheduler.enqueue(p_valid));
    ASSERT_THROW(scheduler.enqueue(p_invalid_q_id), std::out_of_range);
}

TEST(WrrSchedulerTest, GetQueueSizeInvalidQueueId) {
    auto configs = createWrrConfigsWithPermissiveAqm({{static_cast<core::QueueId>(1), 10}});
    WrrScheduler scheduler(configs);
    ASSERT_THROW(scheduler.get_queue_size(2), std::out_of_range);
    ASSERT_NO_THROW(scheduler.get_queue_size(1));
    ASSERT_EQ(scheduler.get_queue_size(1),0);
}

TEST(WrrSchedulerTest, BasicWeightDistribution) {
    auto configs = createWrrConfigsWithPermissiveAqm({
        {static_cast<core::QueueId>(1), 1}, {static_cast<core::QueueId>(2), 2}
    });
    WrrScheduler scheduler(configs);

    for (int i = 0; i < 3; ++i) scheduler.enqueue(createWrrTestPacket(10 + i, 100, 1));
    for (int i = 0; i < 6; ++i) scheduler.enqueue(createWrrTestPacket(20 + i, 100, 2));

    ASSERT_EQ(scheduler.get_queue_size(1), 3);
    ASSERT_EQ(scheduler.get_queue_size(2), 6);

    std::map<core::QueueId, int> dequeued_counts;
    for (int i = 0; i < 9; ++i) {
        dequeued_counts[static_cast<core::QueueId>(scheduler.dequeue().priority)]++;
    }
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(1)], 3);
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(2)], 6);
}

TEST(WrrSchedulerTest, WeightDistributionSpecificOrderOneRound) {
     auto configs = createWrrConfigsWithPermissiveAqm({
        {static_cast<core::QueueId>(1), 1}, {static_cast<core::QueueId>(2), 2}
    });
    WrrScheduler scheduler2(configs);
    scheduler2.enqueue(createWrrTestPacket(10,100,1));
    scheduler2.enqueue(createWrrTestPacket(20,100,2));
    scheduler2.enqueue(createWrrTestPacket(21,100,2));

    std::map<core::QueueId, int> round_counts;
    round_counts[static_cast<core::QueueId>(scheduler2.dequeue().priority)]++;
    round_counts[static_cast<core::QueueId>(scheduler2.dequeue().priority)]++;
    round_counts[static_cast<core::QueueId>(scheduler2.dequeue().priority)]++;

    ASSERT_EQ(round_counts[1], 1);
    ASSERT_EQ(round_counts[2], 2);
}

TEST(WrrSchedulerTest, WeightDistributionWithEmptyQueues) {
    auto configs = createWrrConfigsWithPermissiveAqm({
        {static_cast<core::QueueId>(1), 1}, {static_cast<core::QueueId>(2), 3}
    });
    WrrScheduler scheduler(configs);

    for (int i = 0; i < 5; ++i) scheduler.enqueue(createWrrTestPacket(20 + i, 100, 2));
    ASSERT_EQ(scheduler.get_queue_size(1), 0);
    ASSERT_EQ(scheduler.get_queue_size(2), 5);

    for (int i = 0; i < 5; ++i) {
        ASSERT_EQ(static_cast<core::QueueId>(scheduler.dequeue().priority), static_cast<core::QueueId>(2));
    }
    ASSERT_TRUE(scheduler.is_empty());

    scheduler.enqueue(createWrrTestPacket(10, 100, 1));
    for (int i = 0; i < 6; ++i) scheduler.enqueue(createWrrTestPacket(30 + i, 100, 2));

    ASSERT_EQ(static_cast<core::QueueId>(scheduler.dequeue().priority), static_cast<core::QueueId>(1));
    for(int i=0; i<3; ++i) ASSERT_EQ(static_cast<core::QueueId>(scheduler.dequeue().priority), static_cast<core::QueueId>(2));
    for(int i=0; i<3; ++i) ASSERT_EQ(static_cast<core::QueueId>(scheduler.dequeue().priority), static_cast<core::QueueId>(2));
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(WrrSchedulerTest, ComplexDistributionMultipleRounds) {
    auto configs = createWrrConfigsWithPermissiveAqm({
        {static_cast<core::QueueId>(1), 1},
        {static_cast<core::QueueId>(2), 2},
        {static_cast<core::QueueId>(3), 3}
    });
    WrrScheduler scheduler(configs);
    for (int i = 0; i < 2; ++i) scheduler.enqueue(createWrrTestPacket(100 + i, 100, 1));
    for (int i = 0; i < 4; ++i) scheduler.enqueue(createWrrTestPacket(200 + i, 100, 2));
    for (int i = 0; i < 6; ++i) scheduler.enqueue(createWrrTestPacket(300 + i, 100, 3));

    std::map<core::QueueId, int> dequeued_counts;
    for (int i = 0; i < 12; ++i) {
        dequeued_counts[static_cast<core::QueueId>(scheduler.dequeue().priority)]++;
    }
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(1)], 2);
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(2)], 4);
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(3)], 6);
}

// --- New Tests for AQM Behavior with WRR ---

TEST(WrrSchedulerAqmTest, AqmDropInWrrQueue) {
    core::QueueId qid_normal = 0;
    core::QueueId qid_lossy = 1;
    uint32_t normal_quantum = 10; // High weight for normal queue
    uint32_t lossy_quantum = 1;   // Low weight for lossy queue

    RedAqmParameters permissive_params = createPermissiveAqmParams();
    RedAqmParameters aggressive_params = RedAqmParameters(50, 100, 0.5, 1.0, 150); // min,max,max_p,weight,cap_bytes

    std::vector<WrrScheduler::QueueConfig> configs = {
        {qid_normal, normal_quantum, permissive_params},
        {qid_lossy, lossy_quantum, aggressive_params}
    };
    WrrScheduler scheduler(configs);

    int normal_queue_initial_count = 20; // Enqueue more to give lossy queue turns
    for (int i = 0; i < normal_queue_initial_count; ++i) {
        scheduler.enqueue(createWrrTestPacket(i, 10, qid_normal)); // Small packets
    }
    ASSERT_EQ(scheduler.get_queue_size(qid_normal), normal_queue_initial_count);

    int attempts_on_lossy_queue = 50;
    for (int i = 0; i < attempts_on_lossy_queue; ++i) {
        scheduler.enqueue(createWrrTestPacket(1000 + i, 10, qid_lossy));
    }

    size_t lossy_queue_actual_count = scheduler.get_queue_size(qid_lossy);
    ASSERT_LT(lossy_queue_actual_count, attempts_on_lossy_queue);
    ASSERT_GT(lossy_queue_actual_count, 0); // Expect some to get through initially

    // Dequeue all packets and check final counts
    std::map<core::QueueId, int> final_counts;
    while(!scheduler.is_empty()){
        final_counts[static_cast<core::QueueId>(scheduler.dequeue().priority)]++;
    }
    ASSERT_EQ(final_counts[qid_normal], normal_queue_initial_count);
    ASSERT_EQ(final_counts[qid_lossy], lossy_queue_actual_count);
}

TEST(WrrSchedulerAqmTest, AqmPhysicalCapacityDropInWrrQueue) {
    core::QueueId qid_small_cap = 0;
    core::QueueId qid_normal_cap = 1;

    RedAqmParameters small_cap_params(80, 90, 0.1, 0.002, 100); // Capacity 100 bytes
    RedAqmParameters normal_cap_params = createPermissiveAqmParams(1000);

    std::vector<WrrScheduler::QueueConfig> configs = {
        {qid_small_cap, 1, small_cap_params}, // weight 1
        {qid_normal_cap, 1, normal_cap_params} // weight 1
    };
    WrrScheduler scheduler(configs);

    scheduler.enqueue(createWrrTestPacket(1, 50, qid_small_cap));
    scheduler.enqueue(createWrrTestPacket(2, 50, qid_small_cap));
    ASSERT_EQ(scheduler.get_queue_size(qid_small_cap), 2); // Should be 2*50 = 100 bytes

    size_t total_packets_before = scheduler.get_queue_size(qid_small_cap) + scheduler.get_queue_size(qid_normal_cap);
    scheduler.enqueue(createWrrTestPacket(3, 10, qid_small_cap)); // This should be dropped by physical capacity
    size_t total_packets_after = scheduler.get_queue_size(qid_small_cap) + scheduler.get_queue_size(qid_normal_cap);

    ASSERT_EQ(total_packets_before, total_packets_after);
    ASSERT_EQ(scheduler.get_queue_size(qid_small_cap), 2);

    scheduler.enqueue(createWrrTestPacket(4, 100, qid_normal_cap));
    ASSERT_EQ(scheduler.get_queue_size(qid_normal_cap), 1);
}


} // namespace scheduler
} // namespace hqts
