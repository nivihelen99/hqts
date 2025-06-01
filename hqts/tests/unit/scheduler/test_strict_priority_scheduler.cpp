#include "gtest/gtest.h"
#include "hqts/scheduler/strict_priority_scheduler.h"
#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor
#include "hqts/core/flow_context.h"          // For core::FlowId (used in PacketDescriptor)

#include <vector>
#include <stdexcept> // For std::invalid_argument, std::out_of_range, std::runtime_error

namespace hqts {
namespace scheduler {

// Helper to create a PacketDescriptor for tests
PacketDescriptor createTestPacket(core::FlowId flow_id, uint32_t length, uint8_t priority, size_t payload_size = 0) {
    // Assuming PacketDescriptor constructor: PacketDescriptor(core::FlowId f_id, uint32_t len, uint8_t prio, size_t payload_size = 0)
    return PacketDescriptor(flow_id, length, priority, payload_size);
}

TEST(StrictPrioritySchedulerTest, Constructor) {
    ASSERT_NO_THROW(StrictPriorityScheduler scheduler(8));
    ASSERT_THROW(StrictPriorityScheduler scheduler_invalid(0), std::invalid_argument);

    StrictPriorityScheduler scheduler(4);
    ASSERT_EQ(scheduler.get_num_priority_levels(), 4);
    ASSERT_TRUE(scheduler.is_empty());
    // Check initial queue sizes
    for (uint8_t i = 0; i < 4; ++i) {
        ASSERT_EQ(scheduler.get_queue_size(i), 0);
    }
}

TEST(StrictPrioritySchedulerTest, EnqueueAndDequeueSinglePacket) {
    StrictPriorityScheduler scheduler(8);
    PacketDescriptor p1 = createTestPacket(1, 100, 3);

    scheduler.enqueue(p1);
    ASSERT_FALSE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(3), 1);
    // Check other queues are empty
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
    StrictPriorityScheduler scheduler(8);
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_THROW(scheduler.dequeue(), std::runtime_error);
}

TEST(StrictPrioritySchedulerTest, EnqueueInvalidPriority) {
    StrictPriorityScheduler scheduler(4); // Priorities 0, 1, 2, 3
    PacketDescriptor p_valid_high = createTestPacket(1, 100, 3);
    PacketDescriptor p_invalid_too_high = createTestPacket(2, 100, 4);
    PacketDescriptor p_invalid_way_too_high = createTestPacket(3, 100, 255);

    ASSERT_NO_THROW(scheduler.enqueue(p_valid_high));
    ASSERT_THROW(scheduler.enqueue(p_invalid_too_high), std::out_of_range);
    ASSERT_THROW(scheduler.enqueue(p_invalid_way_too_high), std::out_of_range);
}

TEST(StrictPrioritySchedulerTest, GetQueueSizeInvalidPriority) {
    StrictPriorityScheduler scheduler(4);
    ASSERT_THROW(scheduler.get_queue_size(4), std::out_of_range);
    ASSERT_THROW(scheduler.get_queue_size(255), std::out_of_range);
    ASSERT_NO_THROW(scheduler.get_queue_size(0));
    ASSERT_NO_THROW(scheduler.get_queue_size(3));
}

TEST(StrictPrioritySchedulerTest, StrictPriorityOrder) {
    StrictPriorityScheduler scheduler(4); // Prio levels 0, 1, 2, 3 (3 is highest sched prio)
    PacketDescriptor p_low = createTestPacket(10, 100, 0);    // Lowest prio
    PacketDescriptor p_mid = createTestPacket(20, 100, 1);    // Mid prio
    PacketDescriptor p_high = createTestPacket(30, 100, 3);   // Highest prio
    PacketDescriptor p_mid2 = createTestPacket(40, 100, 1);   // Another mid prio

    scheduler.enqueue(p_low);   // Prio 0
    scheduler.enqueue(p_mid);   // Prio 1
    scheduler.enqueue(p_high);  // Prio 3
    scheduler.enqueue(p_mid2);  // Prio 1, enqueued after p_high, but lower prio than p_high

    ASSERT_EQ(scheduler.get_queue_size(0), 1);
    ASSERT_EQ(scheduler.get_queue_size(1), 2);
    ASSERT_EQ(scheduler.get_queue_size(2), 0); // Ensure other queues are empty
    ASSERT_EQ(scheduler.get_queue_size(3), 1);
    ASSERT_FALSE(scheduler.is_empty());

    // Dequeue order should be: p_high (3), p_mid (1), p_mid2 (1), p_low (0)
    PacketDescriptor dequeued;

    dequeued = scheduler.dequeue();
    ASSERT_EQ(dequeued.flow_id, p_high.flow_id);
    ASSERT_EQ(dequeued.priority, 3);
    ASSERT_EQ(scheduler.get_queue_size(3), 0);

    dequeued = scheduler.dequeue();
    ASSERT_EQ(dequeued.flow_id, p_mid.flow_id);
    ASSERT_EQ(dequeued.priority, 1);
    ASSERT_EQ(scheduler.get_queue_size(1), 1);

    dequeued = scheduler.dequeue();
    ASSERT_EQ(dequeued.flow_id, p_mid2.flow_id);
    ASSERT_EQ(dequeued.priority, 1);
    ASSERT_EQ(scheduler.get_queue_size(1), 0);

    dequeued = scheduler.dequeue();
    ASSERT_EQ(dequeued.flow_id, p_low.flow_id);
    ASSERT_EQ(dequeued.priority, 0);
    ASSERT_EQ(scheduler.get_queue_size(0), 0);

    ASSERT_TRUE(scheduler.is_empty());
}

TEST(StrictPrioritySchedulerTest, MultiplePacketsSamePriority) {
    StrictPriorityScheduler scheduler(3); // Prio 0, 1, 2
    PacketDescriptor p1_prio1 = createTestPacket(1, 100, 1);
    PacketDescriptor p2_prio1 = createTestPacket(2, 100, 1);
    PacketDescriptor p3_prio0 = createTestPacket(3, 100, 0);

    scheduler.enqueue(p1_prio1);
    scheduler.enqueue(p3_prio0); // Lower priority, enqueued before p2_prio1
    scheduler.enqueue(p2_prio1); // Same priority as p1, enqueued later

    // Expected order: p1_prio1 (1), p2_prio1 (1) (FIFO within priority), then p3_prio0 (0)
    PacketDescriptor dequeued;

    dequeued = scheduler.dequeue(); // Highest available is Prio 1
    ASSERT_EQ(dequeued.flow_id, p1_prio1.flow_id);
    ASSERT_EQ(dequeued.priority, 1);

    dequeued = scheduler.dequeue(); // Next is also Prio 1
    ASSERT_EQ(dequeued.flow_id, p2_prio1.flow_id);
    ASSERT_EQ(dequeued.priority, 1);

    dequeued = scheduler.dequeue(); // Last is Prio 0
    ASSERT_EQ(dequeued.flow_id, p3_prio0.flow_id);
    ASSERT_EQ(dequeued.priority, 0);

    ASSERT_TRUE(scheduler.is_empty());
}

TEST(StrictPrioritySchedulerTest, FillAndEmptyMultipleLevels) {
    StrictPriorityScheduler scheduler(2); // Prio 0 (low), 1 (high)
    const int num_packets_per_level = 5;

    // Enqueue packets: Prio 1 first, then Prio 0
    for (int i = 0; i < num_packets_per_level; ++i) {
        scheduler.enqueue(createTestPacket(100u + static_cast<core::FlowId>(i), 10, 1)); // High prio
    }
    for (int i = 0; i < num_packets_per_level; ++i) {
        scheduler.enqueue(createTestPacket(200u + static_cast<core::FlowId>(i), 10, 0)); // Low prio
    }

    ASSERT_EQ(scheduler.get_queue_size(1), num_packets_per_level);
    ASSERT_EQ(scheduler.get_queue_size(0), num_packets_per_level);
    ASSERT_FALSE(scheduler.is_empty());

    // Dequeue all high priority packets
    for (int i = 0; i < num_packets_per_level; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        ASSERT_EQ(p.priority, 1);
        ASSERT_EQ(p.flow_id, 100u + static_cast<core::FlowId>(i));
    }
    ASSERT_EQ(scheduler.get_queue_size(1), 0);
    ASSERT_FALSE(scheduler.is_empty()); // Low priority packets should still be there

    // Dequeue all low priority packets
    for (int i = 0; i < num_packets_per_level; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        ASSERT_EQ(p.priority, 0);
        ASSERT_EQ(p.flow_id, 200u + static_cast<core::FlowId>(i));
    }
    ASSERT_EQ(scheduler.get_queue_size(0), 0);
    ASSERT_TRUE(scheduler.is_empty());
}

} // namespace scheduler
} // namespace hqts
