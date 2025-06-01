#include "gtest/gtest.h"
#include "hqts/scheduler/strict_priority_scheduler.h"
#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor
#include "hqts/core/flow_context.h"          // For core::FlowId (used in PacketDescriptor)
#include "hqts/scheduler/aqm_queue.h"        // For RedAqmParameters

#include <vector>
#include <stdexcept> // For std::invalid_argument, std::out_of_range, std::runtime_error
#include <numeric>   // For std::iota if needed

namespace hqts {
namespace scheduler {

// Helper to create a PacketDescriptor for tests
PacketDescriptor createTestPacket(core::FlowId flow_id, uint32_t length, uint8_t priority, size_t payload_size = 0) {
    return PacketDescriptor(flow_id, length, priority, payload_size);
}

// Helper to create a vector of permissive RedAqmParameters for multiple queues
std::vector<RedAqmParameters> createPermissiveParamsList(size_t num_levels,
                                                         uint32_t capacity_bytes = 1000000, // Large capacity
                                                         double max_p = 0.001,             // Very low max drop prob
                                                         double ewma_w = 0.002) {
    std::vector<RedAqmParameters> params_list;
    for (size_t i = 0; i < num_levels; ++i) {
        // Thresholds are very high, so effectively no RED drops for typical test loads
        params_list.emplace_back(
            capacity_bytes * 0.8, // min_threshold (high)
            capacity_bytes * 0.9, // max_threshold (higher)
            max_p,
            ewma_w,
            capacity_bytes
        );
    }
    return params_list;
}


TEST(StrictPrioritySchedulerTest, Constructor) {
    std::vector<RedAqmParameters> empty_params;
    ASSERT_THROW(StrictPriorityScheduler scheduler(empty_params), std::invalid_argument);

    std::vector<RedAqmParameters> params_list = createPermissiveParamsList(2);

    ASSERT_NO_THROW(StrictPriorityScheduler scheduler(params_list));
    StrictPriorityScheduler scheduler(params_list);
    ASSERT_EQ(scheduler.get_num_priority_levels(), 2);
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(0), 0);
    ASSERT_EQ(scheduler.get_queue_size(1), 0);
    ASSERT_THROW(scheduler.get_queue_size(2), std::out_of_range);
}

TEST(StrictPrioritySchedulerTest, EnqueueAndDequeueSinglePacket) {
    StrictPriorityScheduler scheduler(createPermissiveParamsList(8));
    PacketDescriptor p1 = createTestPacket(1, 100, 3);

    scheduler.enqueue(p1); // AQM should accept
    ASSERT_FALSE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(3), 1);

    for (uint8_t i = 0; i < 8; ++i) {
        if (i != 3) {
            ASSERT_EQ(scheduler.get_queue_size(i), 0);
        }
    }

    PacketDescriptor dequeued_p = scheduler.dequeue();
    ASSERT_EQ(dequeued_p.flow_id, p1.flow_id);
    ASSERT_EQ(dequeued_p.priority, p1.priority);
    ASSERT_EQ(dequeued_p.packet_length_bytes, p1.packet_length_bytes);
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(3), 0);
}

TEST(StrictPrioritySchedulerTest, DequeueFromEmpty) {
    StrictPriorityScheduler scheduler(createPermissiveParamsList(8));
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_THROW(scheduler.dequeue(), std::runtime_error);
}

TEST(StrictPrioritySchedulerTest, EnqueueInvalidPriority) {
    StrictPriorityScheduler scheduler(createPermissiveParamsList(4)); // Priorities 0, 1, 2, 3
    PacketDescriptor p_valid_high = createTestPacket(1, 100, 3);
    PacketDescriptor p_invalid_too_high = createTestPacket(2, 100, 4);

    ASSERT_NO_THROW(scheduler.enqueue(p_valid_high));
    ASSERT_THROW(scheduler.enqueue(p_invalid_too_high), std::out_of_range);
}

TEST(StrictPrioritySchedulerTest, GetQueueSizeInvalidPriority) {
    StrictPriorityScheduler scheduler(createPermissiveParamsList(4));
    ASSERT_THROW(scheduler.get_queue_size(4), std::out_of_range);
    ASSERT_NO_THROW(scheduler.get_queue_size(0));
    ASSERT_NO_THROW(scheduler.get_queue_size(3));
}

TEST(StrictPrioritySchedulerTest, StrictPriorityOrder) {
    StrictPriorityScheduler scheduler(createPermissiveParamsList(4));
    PacketDescriptor p_low = createTestPacket(10, 100, 0);
    PacketDescriptor p_mid = createTestPacket(20, 100, 1);
    PacketDescriptor p_high = createTestPacket(30, 100, 3);
    PacketDescriptor p_mid2 = createTestPacket(40, 100, 1);

    scheduler.enqueue(p_low);
    scheduler.enqueue(p_mid);
    scheduler.enqueue(p_high);
    scheduler.enqueue(p_mid2);

    ASSERT_EQ(scheduler.get_queue_size(0), 1);
    ASSERT_EQ(scheduler.get_queue_size(1), 2);
    ASSERT_EQ(scheduler.get_queue_size(3), 1);
    ASSERT_FALSE(scheduler.is_empty());

    PacketDescriptor dequeued;
    dequeued = scheduler.dequeue(); ASSERT_EQ(dequeued.flow_id, p_high.flow_id);
    dequeued = scheduler.dequeue(); ASSERT_EQ(dequeued.flow_id, p_mid.flow_id);
    dequeued = scheduler.dequeue(); ASSERT_EQ(dequeued.flow_id, p_mid2.flow_id);
    dequeued = scheduler.dequeue(); ASSERT_EQ(dequeued.flow_id, p_low.flow_id);
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(StrictPrioritySchedulerTest, MultiplePacketsSamePriority) {
    StrictPriorityScheduler scheduler(createPermissiveParamsList(3));
    PacketDescriptor p1_prio1 = createTestPacket(1, 100, 1);
    PacketDescriptor p2_prio1 = createTestPacket(2, 100, 1);
    PacketDescriptor p3_prio0 = createTestPacket(3, 100, 0);

    scheduler.enqueue(p1_prio1);
    scheduler.enqueue(p3_prio0);
    scheduler.enqueue(p2_prio1);

    PacketDescriptor dequeued;
    dequeued = scheduler.dequeue(); ASSERT_EQ(dequeued.flow_id, p1_prio1.flow_id);
    dequeued = scheduler.dequeue(); ASSERT_EQ(dequeued.flow_id, p2_prio1.flow_id);
    dequeued = scheduler.dequeue(); ASSERT_EQ(dequeued.flow_id, p3_prio0.flow_id);
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(StrictPrioritySchedulerTest, FillAndEmptyMultipleLevels) {
    StrictPriorityScheduler scheduler(createPermissiveParamsList(2));
    const int num_packets_per_level = 5;

    for (int i = 0; i < num_packets_per_level; ++i) {
        scheduler.enqueue(createTestPacket(100u + static_cast<core::FlowId>(i), 10, 1));
    }
    for (int i = 0; i < num_packets_per_level; ++i) {
        scheduler.enqueue(createTestPacket(200u + static_cast<core::FlowId>(i), 10, 0));
    }

    ASSERT_EQ(scheduler.get_queue_size(1), num_packets_per_level);
    ASSERT_EQ(scheduler.get_queue_size(0), num_packets_per_level);

    for (int i = 0; i < num_packets_per_level; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        ASSERT_EQ(p.priority, 1);
        ASSERT_EQ(p.flow_id, 100u + static_cast<core::FlowId>(i));
    }
    for (int i = 0; i < num_packets_per_level; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        ASSERT_EQ(p.priority, 0);
        ASSERT_EQ(p.flow_id, 200u + static_cast<core::FlowId>(i));
    }
    ASSERT_TRUE(scheduler.is_empty());
}

// --- New Tests for AQM Behavior ---

TEST(StrictPrioritySchedulerAqmTest, AqmDropInSpecificQueue) {
    std::vector<RedAqmParameters> params_list;
    // Prio 0: Permissive
    params_list.push_back(RedAqmParameters(8000, 9000, 0.1, 0.002, 10000));
    // Prio 1: Aggressive RED (min_thresh=100, max_thresh=200, capacity=300 bytes, max_p=0.5, w=1.0 for predictability)
    params_list.push_back(RedAqmParameters(100, 200, 0.5, 1.0, 300));
    // Prio 2: Permissive
    params_list.push_back(RedAqmParameters(8000, 9000, 0.1, 0.002, 10000));

    StrictPriorityScheduler scheduler(params_list);

    // Enqueue to Prio 0 and 2 (should be accepted)
    scheduler.enqueue(createTestPacket(1, 100, 0)); // Accepted
    scheduler.enqueue(createTestPacket(2, 100, 2)); // Accepted
    ASSERT_EQ(scheduler.get_queue_size(0), 1);
    ASSERT_EQ(scheduler.get_queue_size(2), 1);

    // Enqueue to Prio 1 (lossy queue)
    // Packet 1 (50B): total=50. avg for next = 50 (w=1). Below min_thresh(100). No drop. Count=1.
    scheduler.enqueue(createTestPacket(101, 50, 1));
    // Packet 2 (50B): total=100. avg for next = 100. At min_thresh. p_b=0. dp with count=1. Assume not dropped. Count=2.
    scheduler.enqueue(createTestPacket(102, 50, 1));
    // Packet 3 (50B): total=150. avg for next = 150. Mid-range. p_b = 0.5 * (150-100)/(200-100) = 0.25. dp with count=2.
    scheduler.enqueue(createTestPacket(103, 50, 1));
    // Packet 4 (50B): total=200. avg for next = 200. At max_thresh. p_b = 0.5. dp with count=3.
    scheduler.enqueue(createTestPacket(104, 50, 1));
    // Packet 5 (50B): total=250. avg for next = 250. At max_thresh. p_b = 0.5. dp with count=?. (Assume one dropped, count reset)
    scheduler.enqueue(createTestPacket(105, 50, 1));
    // Packet 6 (50B): total=300. avg for next = 300. At max_thresh. p_b = 0.5. dp.
    scheduler.enqueue(createTestPacket(106, 50, 1));
    // Packet 7 (10B): total=310 -> physical drop by RedAqmQueue (cap 300)
    bool accepted_p7 = scheduler.enqueue(createTestPacket(107,10,1)); // This packet itself is not added to total_packets_

    size_t prio1_final_count = scheduler.get_queue_size(1);
    // Expect some drops in Prio 1, so less than 6 packets (P7 is physical drop by AQM queue)
    ASSERT_LT(prio1_final_count, 6);
    ASSERT_GT(prio1_final_count, 0); // But not all should be dropped if params are not extremely aggressive

    // Dequeue and check order
    ASSERT_EQ(scheduler.dequeue().priority, 2); // Prio 2 first
    for(size_t i=0; i<prio1_final_count; ++i) {
        ASSERT_EQ(scheduler.dequeue().priority, 1); // Then all from Prio 1
    }
    ASSERT_EQ(scheduler.dequeue().priority, 0); // Then Prio 0
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(StrictPrioritySchedulerAqmTest, AqmPhysicalCapacityDropInSpecificQueue) {
    std::vector<RedAqmParameters> params_list;
    params_list.push_back(RedAqmParameters(80, 90, 0.1, 0.002, 100)); // Prio 0: Small capacity
    params_list.push_back(RedAqmParameters(800, 900, 0.1, 0.002, 1000)); // Prio 1: Larger capacity
    StrictPriorityScheduler scheduler(params_list);

    // Fill Prio 0 to physical capacity (100 bytes)
    scheduler.enqueue(createTestPacket(1, 50, 0));
    scheduler.enqueue(createTestPacket(2, 50, 0));
    ASSERT_EQ(scheduler.get_queue_size(0), 2);
    ASSERT_EQ(scheduler.get_queue_size(1), 0);

    // This should be dropped by Prio 0's AQM due to physical capacity
    // The scheduler's total_packets_ should not increase.
    size_t packets_before = scheduler.get_queue_size(0) + scheduler.get_queue_size(1);
    scheduler.enqueue(createTestPacket(3, 10, 0)); // Attempt to add to Prio 0
    size_t packets_after = scheduler.get_queue_size(0) + scheduler.get_queue_size(1);
    ASSERT_EQ(packets_before, packets_after);
    ASSERT_EQ(scheduler.get_queue_size(0), 2); // Still 2 packets in Prio 0

    // Prio 1 should be unaffected
    scheduler.enqueue(createTestPacket(4, 100, 1));
    ASSERT_EQ(scheduler.get_queue_size(1), 1);
}

TEST(StrictPrioritySchedulerAqmTest, AqmInteractionAcrossPriorities) {
    // P0: Permissive. P1: Aggressive drops.
    std::vector<RedAqmParameters> params_list;
    params_list.push_back(RedAqmParameters(8000, 9000, 0.01, 0.002, 10000)); // P0
    params_list.push_back(RedAqmParameters(50, 100, 0.5, 1.0, 200));      // P1 (lossy)
    StrictPriorityScheduler scheduler(params_list);

    // Cause P1 to start dropping
    for(int i=0; i<10; ++i) scheduler.enqueue(createTestPacket(100+i, 20, 1)); // Fill P1
    size_t p1_initial_count = scheduler.get_queue_size(1);
    ASSERT_LT(p1_initial_count, 10u); // Expect some drops

    // Enqueue to P0, should all be accepted
    for(int i=0; i<5; ++i) scheduler.enqueue(createTestPacket(i, 100, 0));
    ASSERT_EQ(scheduler.get_queue_size(0), 5);

    // Dequeue order
    for(size_t i=0; i<p1_initial_count; ++i) ASSERT_EQ(scheduler.dequeue().priority, 1);
    for(int i=0; i<5; ++i) ASSERT_EQ(scheduler.dequeue().priority, 0);
    ASSERT_TRUE(scheduler.is_empty());
}


} // namespace scheduler
} // namespace hqts
