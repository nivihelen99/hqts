#include "gtest/gtest.h"
#include "hqts/core/traffic_shaper.h"
#include "hqts/core/flow_context.h"
#include "hqts/core/shaping_policy.h"
#include "hqts/policy/policy_tree.h"
#include "hqts/scheduler/packet_descriptor.h" // Includes ConformanceLevel

#include <string>
#include <vector>
#include <iostream> // For potential debug
#include <memory>   // For std::unique_ptr

namespace hqts {
namespace core {

// Helper to create a PacketDescriptor for TrafficShaper tests
scheduler::PacketDescriptor createShaperTestPacket(core::FlowId flow_id, uint32_t length) {
    // Initial priority and conformance will be overridden by TrafficShaper
    return scheduler::PacketDescriptor(flow_id, length, 0, 0, scheduler::ConformanceLevel::GREEN);
}

class TrafficShaperTest : public ::testing::Test {
protected:
    policy::PolicyTree test_policy_tree_;
    std::unique_ptr<TrafficShaper> shaper_; // Use unique_ptr for RAII

    // Default Policy IDs
    const policy::PolicyId POLICY_ID_GREEN_YELLOW_RED = 1;
    const policy::PolicyId POLICY_ID_DROP_RED = 2;
    const policy::PolicyId POLICY_ID_ALLOW_RED_LOW_PRIO = 3;
    const policy::PolicyId POLICY_ID_CIR_ONLY_GREEN = 4;
    const core::FlowId NO_PARENT = 0; // Assuming 0 is used for no parent in policy hierarchy

    void SetUp() override {
        // Policy 1: Standard Green/Yellow/Red, don't drop RED by default from policy
        // CIR=1Mbps (125000 Bps), CBS=1500B. PIR=2Mbps (250000 Bps), PBS=3000B.
        // Priorities G=7, Y=4, R=1. QueueIDs G=10, Y=11, R=12
        ShapingPolicy policy1(
            POLICY_ID_GREEN_YELLOW_RED, NO_PARENT, "Policy_GYR",
            1000000, 2000000, 1500, 3000,
            policy::SchedulingAlgorithm::STRICT_PRIORITY, 100, 0, // algo, weight, base_prio_level
            false,           // p_drop_on_red = false
            7, 4, 1,        // prio_g, prio_y, prio_r
            10, 11, 12      // qid_g, qid_y, qid_r
        );
        test_policy_tree_.insert(policy1);

        // Policy 2: Drop RED packets
        ShapingPolicy policy2(
            POLICY_ID_DROP_RED, NO_PARENT, "Policy_DropR",
            500000, 1000000, 1000, 2000,
            policy::SchedulingAlgorithm::STRICT_PRIORITY, 100, 0,
            true,            // p_drop_on_red = true
            6, 3, 0,        // Prio R is 0, but will be dropped
            20, 21, 22
        );
        test_policy_tree_.insert(policy2);

        // Policy 3: Allow RED packets but at very low priority
        ShapingPolicy policy3(
            POLICY_ID_ALLOW_RED_LOW_PRIO, NO_PARENT, "Policy_AllowR_LowPrio",
            500000, 1000000, 1000, 2000,
            policy::SchedulingAlgorithm::STRICT_PRIORITY, 100, 0,
            false,           // p_drop_on_red = false
            5, 2, 0,        // prio_r = 0 (very low)
            30, 31, 32
        );
        test_policy_tree_.insert(policy3);

        // Policy 4: CIR only (PIR=0 effectively, or PIR=CIR)
        // To make PIR effectively same as CIR, set PIR rate = CIR rate and PBS = CBS
         ShapingPolicy policy4(
            POLICY_ID_CIR_ONLY_GREEN, NO_PARENT, "Policy_CirOnly",
            1000000, 1000000, 1500, 1500, // PIR=CIR, PBS=CBS
            policy::SchedulingAlgorithm::STRICT_PRIORITY, 100, 0,
            true,           // drop_on_red (effectively means drop if > CIR)
            7, 7, 7,        // prio_y, prio_r will effectively not be used if PIR=CIR
            40, 40, 40
        );
        test_policy_tree_.insert(policy4);

        shaper_ = std::make_unique<TrafficShaper>(test_policy_tree_);
    }

    void TearDown() override {
        // shaper_.reset(); // unique_ptr handles this automatically
    }
};


TEST_F(TrafficShaperTest, ProcessPacketGreen) {
    FlowContext flow_ctx(1, POLICY_ID_GREEN_YELLOW_RED, 0, hqts::core::DropPolicy::TAIL_DROP);
    scheduler::PacketDescriptor packet = createShaperTestPacket(1, 1000); // 1000 bytes

    // Policy 1: CIR=1Mbps, CBS=1500B. Packet is 1000B < 1500B. Should be GREEN.
    bool should_enqueue = shaper_->process_packet(packet, flow_ctx);

    ASSERT_TRUE(should_enqueue);
    ASSERT_EQ(packet.conformance, scheduler::ConformanceLevel::GREEN);
    ASSERT_EQ(packet.priority, 7); // target_priority_green for Policy 1
}

TEST_F(TrafficShaperTest, ProcessPacketYellow) {
    FlowContext flow_ctx(1, POLICY_ID_GREEN_YELLOW_RED, 0, hqts::core::DropPolicy::TAIL_DROP);
    scheduler::PacketDescriptor packet1 = createShaperTestPacket(1, 1000);
    scheduler::PacketDescriptor packet2 = createShaperTestPacket(1, 1000);

    ASSERT_TRUE(shaper_->process_packet(packet1, flow_ctx));
    ASSERT_EQ(packet1.conformance, scheduler::ConformanceLevel::GREEN);

    // After P1 (1000B), Policy1 CIR bucket (1500B initially) has 500B left.
    // P2 (1000B) fails CIR (needs 1000B, has 500B).
    // Policy1 PIR bucket (3000B initially) consumed 1000B for P1, has 2000B left.
    // P2 consumes 1000B from PIR. Should be YELLOW.
    bool should_enqueue_p2 = shaper_->process_packet(packet2, flow_ctx);
    ASSERT_TRUE(should_enqueue_p2);
    ASSERT_EQ(packet2.conformance, scheduler::ConformanceLevel::YELLOW);
    ASSERT_EQ(packet2.priority, 4); // target_priority_yellow for Policy 1
}

TEST_F(TrafficShaperTest, ProcessPacketRedAndAllow) {
    FlowContext flow_ctx(1, POLICY_ID_GREEN_YELLOW_RED, 0, hqts::core::DropPolicy::TAIL_DROP); // Policy 1: drop_on_red = false

    // Consume all CIR (1500B) and PIR (3000B) tokens for Policy 1
    // P1 (1000B): GREEN. CIR bucket: 1500-1000=500. PIR bucket: 3000-1000=2000.
    ASSERT_TRUE(shaper_->process_packet(createShaperTestPacket(1, 1000), flow_ctx));
    // P2 (1000B): CIR fails (needs 1000, has 500). PIR ok (needs 1000, has 2000). YELLOW.
    //             PIR bucket: 2000-1000=1000.
    ASSERT_TRUE(shaper_->process_packet(createShaperTestPacket(1, 1000), flow_ctx));
    // P3 (1000B): CIR fails. PIR ok (needs 1000, has 1000). YELLOW.
    //             PIR bucket: 1000-1000=0.
    ASSERT_TRUE(shaper_->process_packet(createShaperTestPacket(1, 1000), flow_ctx));

    scheduler::PacketDescriptor packet4 = createShaperTestPacket(1, 500); // Next packet
    bool should_enqueue_p4 = shaper_->process_packet(packet4, flow_ctx); // CIR fails. PIR fails. Should be RED.

    ASSERT_TRUE(should_enqueue_p4); // Policy 1 does not drop RED
    ASSERT_EQ(packet4.conformance, scheduler::ConformanceLevel::RED);
    ASSERT_EQ(packet4.priority, 1); // target_priority_red for Policy 1
}

TEST_F(TrafficShaperTest, ProcessPacketRedAndDrop) {
    FlowContext flow_ctx(2, POLICY_ID_DROP_RED, 0, hqts::core::DropPolicy::TAIL_DROP); // Policy 2: drop_on_red = true

    // Policy 2: CIR 0.5Mbps (62500 Bps), CBS 1000B. PIR 1Mbps (125000 Bps), PBS 2000B.
    // P1 (800B): GREEN. CIR: 1000-800=200. PIR: 2000-800=1200.
    ASSERT_TRUE(shaper_->process_packet(createShaperTestPacket(2, 800), flow_ctx));
    // P2 (800B): CIR fails (needs 800, has 200). PIR ok (needs 800, has 1200). YELLOW.
    //             PIR: 1200-800=400.
    ASSERT_TRUE(shaper_->process_packet(createShaperTestPacket(2, 800), flow_ctx));

    scheduler::PacketDescriptor packet3 = createShaperTestPacket(2, 800); // Wants 800B. PIR only 400 left. RED.
    bool should_enqueue_p3 = shaper_->process_packet(packet3, flow_ctx);

    ASSERT_FALSE(should_enqueue_p3); // Policy 2 drops RED
    ASSERT_EQ(packet3.conformance, scheduler::ConformanceLevel::RED);
    // Priority might be set before drop decision, or not, depending on implementation.
    // Current implementation sets priority then drop flag is checked.
    ASSERT_EQ(packet3.priority, 0); // target_priority_red for Policy 2
}

TEST_F(TrafficShaperTest, PolicyNotFound) {
    FlowContext flow_ctx(99, 999, 0, hqts::core::DropPolicy::TAIL_DROP); // Policy 999 does not exist
    scheduler::PacketDescriptor packet = createShaperTestPacket(99, 100);

    bool should_enqueue = shaper_->process_packet(packet, flow_ctx);

    ASSERT_FALSE(should_enqueue); // Expect drop if policy not found
    ASSERT_EQ(packet.conformance, scheduler::ConformanceLevel::RED); // Marked RED by default
}

TEST_F(TrafficShaperTest, CirOnlyPolicy) {
    FlowContext flow_ctx(4, POLICY_ID_CIR_ONLY_GREEN, 0, hqts::core::DropPolicy::TAIL_DROP);
    // Policy 4: CIR=1Mbps, CBS=1500B. PIR=1Mbps, PBS=1500B. drop_on_red=true.

    scheduler::PacketDescriptor packet1 = createShaperTestPacket(4, 1000); // GREEN
    ASSERT_TRUE(shaper_->process_packet(packet1, flow_ctx));
    ASSERT_EQ(packet1.conformance, scheduler::ConformanceLevel::GREEN);
    ASSERT_EQ(packet1.priority, 7);

    scheduler::PacketDescriptor packet2 = createShaperTestPacket(4, 1000); // Exceeds CBS (500 left). CIR fails.
                                                                    // Since PIR=CIR, PIR also fails. Should be RED.
    bool should_enqueue_p2 = shaper_->process_packet(packet2, flow_ctx);
    ASSERT_FALSE(should_enqueue_p2); // drop_on_red is true for policy 4
    ASSERT_EQ(packet2.conformance, scheduler::ConformanceLevel::RED);
     // Priority for RED for policy 4 is 7.
    ASSERT_EQ(packet2.priority, 7);
}

} // namespace core
} // namespace hqts
