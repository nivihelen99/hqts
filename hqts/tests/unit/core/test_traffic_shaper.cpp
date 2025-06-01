#include "gtest/gtest.h"
#include "hqts/core/traffic_shaper.h"
#include "hqts/core/flow_context.h"
#include "hqts/core/shaping_policy.h"
#include "hqts/policy/policy_tree.h"
#include "hqts/scheduler/packet_descriptor.h" // Includes ConformanceLevel
#include "hqts/dataplane/flow_classifier.h"
#include "hqts/dataplane/flow_identifier.h"

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
    core::FlowTable test_flow_table_; // Added
    std::unique_ptr<dataplane::FlowClassifier> test_flow_classifier_; // Added
    std::unique_ptr<TrafficShaper> shaper_;

    // Default Policy IDs
    const policy::PolicyId POLICY_ID_DEFAULT = 999; // A default policy for new flows
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

        // Add a default policy for the classifier to use
        ShapingPolicy default_policy(
            POLICY_ID_DEFAULT, NO_PARENT, "DefaultPolicy",
            1000000, 1000000, 1000, 1000, // 1Mbps, 1000B C/PBS
            policy::SchedulingAlgorithm::STRICT_PRIORITY, 100, 0,
            true, 7,7,7, 50,50,50 // Drop RED, all green/yellow/red map to prio 7, qid 50
        );
        test_policy_tree_.insert(default_policy);

        test_flow_classifier_ = std::make_unique<dataplane::FlowClassifier>(test_flow_table_, POLICY_ID_DEFAULT);
        shaper_ = std::make_unique<TrafficShaper>(test_policy_tree_, *test_flow_classifier_, test_flow_table_);
    }

    void TearDown() override {
        // shaper_ and test_flow_classifier_ will be reset by unique_ptr
    }

    // Helper to set a specific policy for a given 5-tuple for testing
    void set_policy_for_flow(const dataplane::FiveTuple& five_tuple, policy::PolicyId policy_id) {
        core::FlowId flow_id = test_flow_classifier_->get_or_create_flow(five_tuple);
        auto it = test_flow_table_.find(flow_id);
        ASSERT_NE(it, test_flow_table_.end());
        it->second.policy_id = policy_id;
    }
};


TEST_F(TrafficShaperTest, ProcessPacketGreen) {
    dataplane::FiveTuple tuple_gyr(1,2,3,4,6); // Example 5-tuple for this test
    set_policy_for_flow(tuple_gyr, POLICY_ID_GREEN_YELLOW_RED);

    scheduler::PacketDescriptor packet = createShaperTestPacket(0, 1000); // flow_id will be set by shaper

    // Policy 1 (POLICY_ID_GREEN_YELLOW_RED): CIR=1Mbps, CBS=1500B. Packet is 1000B < 1500B. Should be GREEN.
    bool should_enqueue = shaper_->process_packet(packet, tuple_gyr);

    ASSERT_TRUE(should_enqueue);
    ASSERT_EQ(packet.conformance, scheduler::ConformanceLevel::GREEN);
    ASSERT_EQ(packet.flow_id, test_flow_classifier_->get_or_create_flow(tuple_gyr)); // Check if flow_id was set
    ASSERT_EQ(packet.priority, 7); // target_priority_green for Policy 1
}

TEST_F(TrafficShaperTest, ProcessPacketYellow) {
    dataplane::FiveTuple tuple_gyr(1,2,3,4,6);
    set_policy_for_flow(tuple_gyr, POLICY_ID_GREEN_YELLOW_RED);

    scheduler::PacketDescriptor packet1 = createShaperTestPacket(0, 1000);
    scheduler::PacketDescriptor packet2 = createShaperTestPacket(0, 1000);

    ASSERT_TRUE(shaper_->process_packet(packet1, tuple_gyr));
    ASSERT_EQ(packet1.conformance, scheduler::ConformanceLevel::GREEN);

    bool should_enqueue_p2 = shaper_->process_packet(packet2, tuple_gyr);
    ASSERT_TRUE(should_enqueue_p2);
    ASSERT_EQ(packet2.conformance, scheduler::ConformanceLevel::YELLOW);
    ASSERT_EQ(packet2.priority, 4); // target_priority_yellow for Policy 1
}

TEST_F(TrafficShaperTest, ProcessPacketRedAndAllow) {
    dataplane::FiveTuple tuple_gyr(1,2,3,4,6);
    set_policy_for_flow(tuple_gyr, POLICY_ID_GREEN_YELLOW_RED); // Policy 1: drop_on_red = false

    ASSERT_TRUE(shaper_->process_packet(createShaperTestPacket(0, 1000), tuple_gyr));
    ASSERT_TRUE(shaper_->process_packet(createShaperTestPacket(0, 1000), tuple_gyr));
    ASSERT_TRUE(shaper_->process_packet(createShaperTestPacket(0, 1000), tuple_gyr));

    scheduler::PacketDescriptor packet4 = createShaperTestPacket(0, 500);
    bool should_enqueue_p4 = shaper_->process_packet(packet4, tuple_gyr);

    ASSERT_TRUE(should_enqueue_p4);
    ASSERT_EQ(packet4.conformance, scheduler::ConformanceLevel::RED);
    ASSERT_EQ(packet4.priority, 1); // target_priority_red for Policy 1
}

TEST_F(TrafficShaperTest, ProcessPacketRedAndDrop) {
    dataplane::FiveTuple tuple_drop_r(3,4,5,6,6);
    set_policy_for_flow(tuple_drop_r, POLICY_ID_DROP_RED); // Policy 2: drop_on_red = true

    ASSERT_TRUE(shaper_->process_packet(createShaperTestPacket(0, 800), tuple_drop_r));
    ASSERT_TRUE(shaper_->process_packet(createShaperTestPacket(0, 800), tuple_drop_r));

    scheduler::PacketDescriptor packet3 = createShaperTestPacket(0, 800);
    bool should_enqueue_p3 = shaper_->process_packet(packet3, tuple_drop_r);

    ASSERT_FALSE(should_enqueue_p3);
    ASSERT_EQ(packet3.conformance, scheduler::ConformanceLevel::RED);
    ASSERT_EQ(packet3.priority, 0); // target_priority_red for Policy 2
}

TEST_F(TrafficShaperTest, PolicyNotFoundViaDefault) {
    // FlowClassifier will assign POLICY_ID_DEFAULT if this tuple is new
    dataplane::FiveTuple tuple_new(10,20,30,40,6);
    scheduler::PacketDescriptor packet = createShaperTestPacket(0, 100);

    // To specifically test "policy not found in tree" (not just default policy):
    // We need a FlowContext that refers to a policy_id that is NOT in test_policy_tree_
    // This is harder now that FlowClassifier creates the FlowContext with a default_policy_id
    // which we added to the tree.
    // This test's original intent is now covered by how FlowClassifier works: it *always* assigns
    // a policy_id (the default one). If that default_policy_id was NOT in policy_tree, then
    // shaper would fail. Our current setup ensures default_policy_id IS in the tree.
    // So, this test now effectively tests behavior with the default policy.

    bool should_enqueue = shaper_->process_packet(packet, tuple_new);

    // Assuming POLICY_ID_DEFAULT is set to drop_on_red=true and marks RED packets for drop
    // Default policy: drop_on_red = true. Pkt 100B. CIR=1Mbps, CBS=1000B. -> GREEN
    ASSERT_TRUE(should_enqueue);
    ASSERT_EQ(packet.conformance, scheduler::ConformanceLevel::GREEN);
    ASSERT_EQ(packet.priority, 7); // From POLICY_ID_DEFAULT
}

TEST_F(TrafficShaperTest, CirOnlyPolicy) {
    dataplane::FiveTuple tuple_cir_only(5,6,7,8,6);
    set_policy_for_flow(tuple_cir_only, POLICY_ID_CIR_ONLY_GREEN);

    scheduler::PacketDescriptor packet1 = createShaperTestPacket(0, 1000);
    ASSERT_TRUE(shaper_->process_packet(packet1, tuple_cir_only));
    ASSERT_EQ(packet1.conformance, scheduler::ConformanceLevel::GREEN);
    ASSERT_EQ(packet1.priority, 7);

    scheduler::PacketDescriptor packet2 = createShaperTestPacket(0, 1000);
    bool should_enqueue_p2 = shaper_->process_packet(packet2, tuple_cir_only);
    ASSERT_FALSE(should_enqueue_p2);
    ASSERT_EQ(packet2.conformance, scheduler::ConformanceLevel::RED);
    ASSERT_EQ(packet2.priority, 7); // Prio for RED for Policy 4 is 7
}

} // namespace core
} // namespace hqts
