#include "gtest/gtest.h"
#include "hqts/scheduler/hfsc_scheduler.h"
#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor
#include "hqts/core/flow_context.h"          // For core::FlowId

#include <vector>
#include <stdexcept> // For std::logic_error, std::invalid_argument, std::out_of_range

namespace hqts {
namespace scheduler {

// Helper to create a PacketDescriptor. Priority field is used as FlowId.
PacketDescriptor createHfscTestPacket(core::FlowId flow_id, uint32_t length) {
    // Ensure flow_id fits into uint8_t if it's used for packet.priority directly
    if (flow_id > UINT8_MAX) {
        // This test setup assumes flow_id can be represented by packet.priority.
        // If FlowId can be larger, the mapping needs to be handled or PacketDescriptor updated.
        // For these basic tests, we'll assume flow_id values fit.
    }
    return PacketDescriptor(flow_id, length, static_cast<uint8_t>(flow_id));
}

// Helper to create a default ServiceCurve for tests
ServiceCurve defaultRtSc() { return ServiceCurve(1000000, 0); } // 1 Mbps, 0 delay
ServiceCurve defaultLsSc() { return ServiceCurve(500000, 0); }  // 0.5 Mbps, 0 delay
ServiceCurve defaultUlSc() { return ServiceCurve(2000000, 0); } // 2 Mbps, 0 delay

// Test fixture for HfscScheduler tests if common setup is needed later.
// For now, using TEST directly as setup is minimal per test.
// class HfscSchedulerTest : public ::testing::Test {
// protected:
// HfscScheduler scheduler; // Requires initialization in Test::SetUp or per test
// };

TEST(HfscSchedulerTest, ConstructorEmptyConfig) {
    std::vector<HfscScheduler::FlowConfig> empty_configs;
    // Constructor with empty config sets is_configured_ to false
    HfscScheduler scheduler(empty_configs, 1000000000); // 1 Gbps link

    ASSERT_TRUE(scheduler.is_empty()); // is_empty checks total_packets_ which is 0
    ASSERT_EQ(scheduler.get_num_configured_flows(), 0);

    // Enqueue/Dequeue on a scheduler constructed with empty config should throw std::logic_error
    // because its is_configured_ flag will be false.
    ASSERT_THROW(scheduler.enqueue(createHfscTestPacket(1, 100)), std::logic_error);
    ASSERT_THROW(scheduler.dequeue(), std::logic_error);
}

TEST(HfscSchedulerTest, ConstructorValidConfig) {
    std::vector<HfscScheduler::FlowConfig> configs = {
        {static_cast<core::FlowId>(1), defaultRtSc(), defaultLsSc()},
        {static_cast<core::FlowId>(2), ServiceCurve(2000000, 100), ServiceCurve(1000000, 0)}
    };
    HfscScheduler scheduler(configs, 1000000000); // 1 Gbps link

    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_num_configured_flows(), 2);
    ASSERT_NO_THROW(scheduler.get_flow_queue_size(1));
    ASSERT_EQ(scheduler.get_flow_queue_size(1), 0);
    ASSERT_NO_THROW(scheduler.get_flow_queue_size(2));
    ASSERT_EQ(scheduler.get_flow_queue_size(2), 0);
    ASSERT_THROW(scheduler.get_flow_queue_size(3), std::out_of_range); // Flow 3 not configured
}

TEST(HfscSchedulerTest, ConstructorDuplicateFlowId) {
    std::vector<HfscScheduler::FlowConfig> configs = {
        {static_cast<core::FlowId>(1), defaultRtSc()},
        {static_cast<core::FlowId>(1), defaultRtSc()} // Duplicate Flow ID
    };
    ASSERT_THROW(HfscScheduler scheduler(configs, 1000000000), std::invalid_argument);
}

TEST(HfscSchedulerTest, EnqueueAndDequeueSinglePacket_PlaceholderLogic) {
    std::vector<HfscScheduler::FlowConfig> configs = {{static_cast<core::FlowId>(1), defaultRtSc()}};
    HfscScheduler scheduler(configs, 1000000000);

    PacketDescriptor p1 = createHfscTestPacket(1, 100);
    ASSERT_NO_THROW(scheduler.enqueue(p1));
    ASSERT_FALSE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_flow_queue_size(1), 1);

    PacketDescriptor dequeued_p = scheduler.dequeue();
    ASSERT_EQ(dequeued_p.flow_id, p1.flow_id);
    ASSERT_EQ(static_cast<core::FlowId>(dequeued_p.priority), static_cast<core::FlowId>(1)); // Check FlowId mapping
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_flow_queue_size(1), 0);
}

TEST(HfscSchedulerTest, DequeueFromEmpty_PlaceholderLogic) {
    std::vector<HfscScheduler::FlowConfig> configs = {{static_cast<core::FlowId>(1), defaultRtSc()}};
    HfscScheduler scheduler(configs, 1000000000);
    ASSERT_TRUE(scheduler.is_empty());
    // Placeholder dequeue throws runtime_error if empty but configured
    ASSERT_THROW(scheduler.dequeue(), std::runtime_error);
}

TEST(HfscSchedulerTest, EnqueueToUnconfiguredFlowId_PlaceholderLogic) {
    // Only flow 1 configured
    std::vector<HfscScheduler::FlowConfig> configs = {{static_cast<core::FlowId>(1), defaultRtSc()}};
    HfscScheduler scheduler(configs, 1000000000);

    // Flow 2 not configured, packet.priority will be 2 (used as FlowId)
    PacketDescriptor p_unconfigured_flow = createHfscTestPacket(2, 100);
    ASSERT_THROW(scheduler.enqueue(p_unconfigured_flow), std::out_of_range);
}

TEST(HfscSchedulerTest, MultipleFlowsSimple_PlaceholderLogic) {
    std::vector<HfscScheduler::FlowConfig> configs = {
        {static_cast<core::FlowId>(1), defaultRtSc()},
        {static_cast<core::FlowId>(2), defaultRtSc()}
    };
    HfscScheduler scheduler(configs, 1000000000);

    PacketDescriptor p_flow1 = createHfscTestPacket(1, 100);
    PacketDescriptor p_flow2 = createHfscTestPacket(2, 150);

    scheduler.enqueue(p_flow1);
    scheduler.enqueue(p_flow2);
    ASSERT_EQ(scheduler.get_flow_queue_size(1), 1);
    ASSERT_EQ(scheduler.get_flow_queue_size(2), 1);
    ASSERT_FALSE(scheduler.is_empty());

    // Placeholder dequeue is simple FIFO across flows map iteration order.
    // std::map iterates by key, so FlowId 1 should be first.
    PacketDescriptor dp1 = scheduler.dequeue();
    ASSERT_EQ(dp1.flow_id, static_cast<core::FlowId>(1));
    ASSERT_EQ(scheduler.get_flow_queue_size(1), 0);
    ASSERT_EQ(scheduler.get_flow_queue_size(2), 1);


    PacketDescriptor dp2 = scheduler.dequeue();
    ASSERT_EQ(dp2.flow_id, static_cast<core::FlowId>(2));
    ASSERT_EQ(scheduler.get_flow_queue_size(2), 0);

    ASSERT_TRUE(scheduler.is_empty());
}

} // namespace scheduler
} // namespace hqts
