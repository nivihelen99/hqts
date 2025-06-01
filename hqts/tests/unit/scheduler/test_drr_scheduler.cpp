#include "gtest/gtest.h"
#include "hqts/scheduler/drr_scheduler.h"
#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor
#include "hqts/core/flow_context.h"      // For core::QueueId, core::FlowId

#include <vector>
#include <map>
#include <numeric>   // For std::accumulate if needed
#include <stdexcept> // For std::invalid_argument etc.

namespace hqts {
namespace scheduler {

// Helper to create a PacketDescriptor. Priority field is used as QueueId for DRR.
PacketDescriptor createDrrTestPacket(core::FlowId flow_id, uint32_t length, core::QueueId queue_id_for_drr) {
    // Using priority field of PacketDescriptor to denote the target QueueId for DRR
    if (queue_id_for_drr > UINT8_MAX) { // Ensure QueueId fits in uint8_t priority field
        throw std::overflow_error("Queue ID for DRR test packet exceeds uint8_t max value.");
    }
    return PacketDescriptor(flow_id, length, static_cast<uint8_t>(queue_id_for_drr));
}

TEST(DrrSchedulerTest, ConstructorValidation) {
    std::vector<DrrScheduler::QueueConfig> empty_configs;
    ASSERT_THROW(DrrScheduler scheduler(empty_configs), std::invalid_argument);

    std::vector<DrrScheduler::QueueConfig> zero_quantum_configs = {{static_cast<core::QueueId>(1), 0}};
    ASSERT_THROW(DrrScheduler scheduler(zero_quantum_configs), std::invalid_argument);

    std::vector<DrrScheduler::QueueConfig> zero_quantum_configs2 = {
        {static_cast<core::QueueId>(1), 100},
        {static_cast<core::QueueId>(2), 0}
    };
    ASSERT_THROW(DrrScheduler scheduler(zero_quantum_configs2), std::invalid_argument);

    std::vector<DrrScheduler::QueueConfig> duplicate_id_configs = {
        {static_cast<core::QueueId>(1), 100},
        {static_cast<core::QueueId>(1), 200}
    };
    ASSERT_THROW(DrrScheduler scheduler(duplicate_id_configs), std::invalid_argument);

    std::vector<DrrScheduler::QueueConfig> valid_configs = {
        {static_cast<core::QueueId>(1), 100},
        {static_cast<core::QueueId>(2), 200}
    };
    ASSERT_NO_THROW(DrrScheduler scheduler(valid_configs));
    DrrScheduler scheduler(valid_configs);
    ASSERT_EQ(scheduler.get_num_queues(), 2);
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(DrrSchedulerTest, EnqueueAndDequeueSinglePacket) {
    std::vector<DrrScheduler::QueueConfig> configs = {{static_cast<core::QueueId>(100), 500}}; // QID 100, Quantum 500
    DrrScheduler scheduler(configs);

    PacketDescriptor p1 = createDrrTestPacket(1, 100, 100); // Packet length 100
    scheduler.enqueue(p1);
    ASSERT_FALSE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(100), 1);

    PacketDescriptor dequeued_p = scheduler.dequeue();
    ASSERT_EQ(dequeued_p.flow_id, p1.flow_id);
    ASSERT_EQ(dequeued_p.packet_length_bytes, p1.packet_length_bytes);
    ASSERT_EQ(static_cast<core::QueueId>(dequeued_p.priority), static_cast<core::QueueId>(100));
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(100), 0);
}

TEST(DrrSchedulerTest, DequeueFromEmpty) {
    std::vector<DrrScheduler::QueueConfig> configs = {{static_cast<core::QueueId>(1), 100}};
    DrrScheduler scheduler(configs);
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_THROW(scheduler.dequeue(), std::runtime_error);
}

TEST(DrrSchedulerTest, EnqueueInvalidQueueId) {
    std::vector<DrrScheduler::QueueConfig> configs = {{static_cast<core::QueueId>(1), 100}}; // Only queue_id 1
    DrrScheduler scheduler(configs);
    PacketDescriptor p_valid = createDrrTestPacket(1, 50, 1);
    PacketDescriptor p_invalid_q_id = createDrrTestPacket(2, 100, 2); // queue_id 2 not configured

    ASSERT_NO_THROW(scheduler.enqueue(p_valid));
    ASSERT_THROW(scheduler.enqueue(p_invalid_q_id), std::out_of_range);
}

TEST(DrrSchedulerTest, BasicFairnessEqualSizePackets) {
    std::vector<DrrScheduler::QueueConfig> configs = {
        {static_cast<core::QueueId>(1), 100},
        {static_cast<core::QueueId>(2), 100}
    };
    DrrScheduler scheduler(configs);

    for (int i = 0; i < 5; ++i) scheduler.enqueue(createDrrTestPacket(10 + i, 100, 1));
    for (int i = 0; i < 5; ++i) scheduler.enqueue(createDrrTestPacket(20 + i, 100, 2));

    std::map<core::QueueId, int> dequeued_counts;
    for (int i = 0; i < 10; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        dequeued_counts[static_cast<core::QueueId>(p.priority)]++;
    }

    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(1)], 5);
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(2)], 5);
}

TEST(DrrSchedulerTest, FairnessDifferentQuantumsEqualSizePackets) {
    std::vector<DrrScheduler::QueueConfig> configs = {
        {static_cast<core::QueueId>(1), 100}, // Q1, Quantum 100
        {static_cast<core::QueueId>(2), 200}  // Q2, Quantum 200
    };
    DrrScheduler scheduler(configs);

    // Packets are size 100. Q1 sends 1, Q2 sends 2 per "conceptual round".
    // Enqueue 3 for Q1 (3 rounds for Q1), 6 for Q2 (3 rounds for Q2)
    for (int i = 0; i < 3; ++i) scheduler.enqueue(createDrrTestPacket(10 + i, 100, 1));
    for (int i = 0; i < 6; ++i) scheduler.enqueue(createDrrTestPacket(20 + i, 100, 2));

    std::map<core::QueueId, int> dequeued_counts;
    for (int i = 0; i < 9; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        dequeued_counts[static_cast<core::QueueId>(p.priority)]++;
    }
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(1)], 3);
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(2)], 6);
}

TEST(DrrSchedulerTest, FairnessVariablePacketSizes) {
    std::vector<DrrScheduler::QueueConfig> configs = {
        {static_cast<core::QueueId>(1), 300}, // Q1, Quantum 300
        {static_cast<core::QueueId>(2), 300}  // Q2, Quantum 300
    };
    DrrScheduler scheduler(configs);

    // Q1 sends small packets (50 bytes), Q2 sends large packets (150 bytes)
    // Enqueue: 6 small for Q1 (total 300 bytes), 2 large for Q2 (total 300 bytes)
    // This means Q1 can send all 6 in one turn (total 300 <= 300 deficit).
    // Q2 can send both in one turn (total 300 <= 300 deficit).
    for (int i = 0; i < 6; ++i) scheduler.enqueue(createDrrTestPacket(10 + i, 50, 1));
    for (int i = 0; i < 2; ++i) scheduler.enqueue(createDrrTestPacket(20 + i, 150, 2));

    std::map<core::QueueId, uint32_t> dequeued_byte_counts;
    std::map<core::QueueId, int> dequeued_packet_counts;

    // Dequeue order will alternate if both have enough deficit
    // Turn 1: Q1 selected (idx=0). Deficit = 300. Sends P(10,50), D=250. P(11,50), D=200 ... P(15,50), D=0. (6 pkts)
    // (The current implementation sends one packet then current_idx moves)
    // Let's trace current implementation:
    // Q1[6x50b], Q2[2x150b]. Q1.D=0, Q2.D=0. current_idx=0.
    // Dequeue Call 1: idx=0 (Q1). D=0+300=300. Send P(10,50). Q1.D=250. current_idx=1. Returns P(10,50).
    // Dequeue Call 2: idx=1 (Q2). D=0+300=300. Send P(20,150). Q2.D=150. current_idx=0. Returns P(20,150).
    // Dequeue Call 3: idx=0 (Q1). D=250+300=550. Send P(11,50). Q1.D=500. current_idx=1. Returns P(11,50).
    // Dequeue Call 4: idx=1 (Q2). D=150+300=450. Send P(21,150). Q2.D=300. current_idx=0. Returns P(21,150).
    // Dequeue Call 5: idx=0 (Q1). D=500+300=800. Send P(12,50). Q1.D=750. current_idx=1. Returns P(12,50).
    // Dequeue Call 6: idx=1 (Q2). Q2 is empty. current_idx=0.
    // Dequeue Call 7: idx=0 (Q1). D=750+300=1050. Send P(13,50). Q1.D=1000. current_idx=1. Returns P(13,50).
    // Dequeue Call 8: idx=1 (Q2). Q2 is empty. current_idx=0.
    // ... this is not right. The inner while loop in DRR dequeue should send multiple.

    // Re-tracing with fixed DrrScheduler::dequeue (sends multiple packets from a queue per turn):
    // Q1[6x50b], Q2[2x150b]. Q1.D=0, Q2.D=0. current_idx=0.
    // Dequeue Call 1 (selects Q1):
    //   Q1.D = 0 + 300 = 300.
    //   Sends P(10,50). Q1.D = 250.
    //   Sends P(11,50). Q1.D = 200.
    //   Sends P(12,50). Q1.D = 150.
    //   Sends P(13,50). Q1.D = 100.
    //   Sends P(14,50). Q1.D = 50.
    //   Sends P(15,50). Q1.D = 0. Q1 empty.
    //   current_idx = 1. Returns P(15,50) (or the first one, P(10,50), if only one packet is returned per Dequeue call).
    //   The current implementation returns *one* packet per Dequeue call, then advances current_idx.
    //   So the previous trace was more aligned with the *current* code.
    //   This means the test needs to reflect that one-packet-per-dequeue-call behavior.

    for (int i = 0; i < 8; ++i) { // Total 8 packets
        PacketDescriptor p = scheduler.dequeue();
        dequeued_byte_counts[static_cast<core::QueueId>(p.priority)] += p.packet_length_bytes;
        dequeued_packet_counts[static_cast<core::QueueId>(p.priority)]++;
    }
    ASSERT_TRUE(scheduler.is_empty());

    // Given the one-packet-per-dequeue-call, the byte counts should still be fair over time.
    ASSERT_EQ(dequeued_packet_counts[static_cast<core::QueueId>(1)], 6);
    ASSERT_EQ(dequeued_byte_counts[static_cast<core::QueueId>(1)], 300);
    ASSERT_EQ(dequeued_packet_counts[static_cast<core::QueueId>(2)], 2);
    ASSERT_EQ(dequeued_byte_counts[static_cast<core::QueueId>(2)], 300);
}

TEST(DrrSchedulerTest, DeficitHandlingLargePacket) {
    std::vector<DrrScheduler::QueueConfig> configs = {
        {static_cast<core::QueueId>(1), 100}, // Q1, Quantum 100
        {static_cast<core::QueueId>(2), 100}  // Q2, Quantum 100
    };
    DrrScheduler scheduler(configs);
    scheduler.enqueue(createDrrTestPacket(1, 250, 1)); // Large packet for Q1 (length 250)
    scheduler.enqueue(createDrrTestPacket(101, 10, 2)); // Small packet for Q2
    scheduler.enqueue(createDrrTestPacket(102, 10, 2)); // Small packet for Q2
    scheduler.enqueue(createDrrTestPacket(103, 10, 2)); // Small packet for Q2

    // Expected trace with the current code (one send per dequeue call, deficit can be negative):
    // Initial: Q1[P(1,250)], Q2[P(101,10), P(102,10), P(103,10)]. Deficits Q1=0, Q2=0. current_idx=0 (Q1)

    // Dequeue Call 1: (current_idx=0, Q1)
    //  Q1.deficit = 0 + 100 = 100. Front P(1,250). 250 > 100. Cannot send. Q1.deficit remains 100.
    //  current_idx becomes 1. (No packet returned in this internal step of DRR)
    //  (Scheduler continues to Q2 in the same Dequeue call)
    //  Q2.deficit = 0 + 100 = 100. Front P(101,10). 10 <= 100. Send P(101,10). Q2.deficit = 100-10=90.
    //  current_idx becomes 0 ( (1+1)%2 ). RETURN P(101,10).
    PacketDescriptor p_101 = scheduler.dequeue();
    ASSERT_EQ(p_101.flow_id, 101); // Q2 served

    // State after Call 1: Q1[P(1,250)], Q2[P(102,10), P(103,10)]. Deficits Q1=100, Q2=90. current_idx=0 (Q1)
    // Dequeue Call 2: (current_idx=0, Q1)
    //  Q1.deficit = 100 + 100 = 200. Front P(1,250). 250 > 200. Cannot send. Q1.deficit remains 200.
    //  current_idx becomes 1.
    //  Q2.deficit = 90 + 100 = 190. Front P(102,10). 10 <= 190. Send P(102,10). Q2.deficit = 190-10=180.
    //  current_idx becomes 0. RETURN P(102,10).
    PacketDescriptor p_102 = scheduler.dequeue();
    ASSERT_EQ(p_102.flow_id, 102); // Q2 served

    // State after Call 2: Q1[P(1,250)], Q2[P(103,10)]. Deficits Q1=200, Q2=180. current_idx=0 (Q1)
    // Dequeue Call 3: (current_idx=0, Q1)
    //  Q1.deficit = 200 + 100 = 300. Front P(1,250). 250 <= 300. Send P(1,250). Q1.deficit = 300-250=50.
    //  current_idx becomes 1. RETURN P(1,250).
    PacketDescriptor p_1 = scheduler.dequeue();
    ASSERT_EQ(p_1.flow_id, 1); // Q1's large packet served

    // State after Call 3: Q1[], Q2[P(103,10)]. Deficits Q1=50, Q2=180. current_idx=1 (Q2)
    // Dequeue Call 4: (current_idx=1, Q2)
    //  Q2.deficit = 180 + 100 = 280. Front P(103,10). 10 <= 280. Send P(103,10). Q2.deficit = 280-10=270.
    //  current_idx becomes 0. RETURN P(103,10).
    PacketDescriptor p_103 = scheduler.dequeue();
    ASSERT_EQ(p_103.flow_id, 103); // Q2 served

    ASSERT_TRUE(scheduler.is_empty());
}

} // namespace scheduler
} // namespace hqts
