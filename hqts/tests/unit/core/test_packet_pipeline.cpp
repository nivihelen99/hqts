#include "gtest/gtest.h"
#include "hqts/core/packet_pipeline.h"
#include "hqts/dataplane/flow_classifier.h"
#include "hqts/core/traffic_shaper.h"
#include "hqts/scheduler/strict_priority_scheduler.h" // Using SPS with AQM for tests
#include "hqts/scheduler/aqm_queue.h" // For RedAqmParameters
#include "hqts/policy/policy_tree.h"
#include "hqts/core/flow_context.h" // For FlowTable, core::DropPolicy
#include "hqts/dataplane/flow_identifier.h" // For FiveTuple
#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor, ConformanceLevel

#include <memory>   // For std::unique_ptr
#include <vector>
#include <map>      // For std::map in tests

namespace hqts {
namespace core {

// Helper to create a PacketDescriptor (not strictly for pipeline input, but can be useful)
scheduler::PacketDescriptor createPipelineTestPacket(core::FlowId flow_id, uint32_t length, uint8_t priority) {
    return scheduler::PacketDescriptor(flow_id, length, priority);
}

// Helper to create permissive RedAqmParameters for schedulers in tests
std::vector<scheduler::RedAqmParameters> createPermissiveSpsParams(size_t num_levels) {
    std::vector<scheduler::RedAqmParameters> params_list;
    for (size_t i = 0; i < num_levels; ++i) {
        params_list.emplace_back(
            100000, // min_threshold_bytes (very high)
            200000, // max_threshold_bytes (very high)
            0.01,   // max_probability (low)
            0.002,  // ewma_weight
            250000  // queue_capacity_bytes (large)
        );
    }
    return params_list;
}


class PacketPipelineTest : public ::testing::Test {
protected:
    policy::PolicyTree test_policy_tree_;
    core::FlowTable test_flow_table_;

    std::unique_ptr<dataplane::FlowClassifier> classifier_;
    std::unique_ptr<TrafficShaper> shaper_;
    std::unique_ptr<scheduler::StrictPriorityScheduler> main_scheduler_;
    std::unique_ptr<PacketPipeline> pipeline_;

    // Define some policy IDs
    const policy::PolicyId POLICY_ID_HIGH_PRIO = 1;
    const policy::PolicyId POLICY_ID_MID_PRIO = 2;
    const policy::PolicyId POLICY_ID_LOW_PRIO = 3;
    const policy::PolicyId POLICY_ID_DROP_RED_PACKETS = 4;
    const policy::PolicyId POLICY_ID_DEFAULT = 99;
    const core::FlowId NO_PARENT = 0;


    void SetUp() override {
        test_policy_tree_ = policy::PolicyTree();
        test_flow_table_.clear();

        // Setup Policies
        test_policy_tree_.insert(ShapingPolicy(
            POLICY_ID_HIGH_PRIO, NO_PARENT, "HighPrio",
            1000000, 1000000, 1500, 1500,
            policy::SchedulingAlgorithm::STRICT_PRIORITY, 100, 0,
            false, 7, 6, 5, 1, 1, 1
        ));
        test_policy_tree_.insert(ShapingPolicy(
            POLICY_ID_MID_PRIO, NO_PARENT, "MidPrio",
            1000000, 1000000, 1500, 1500,
            policy::SchedulingAlgorithm::STRICT_PRIORITY, 100, 0,
            false, 4, 3, 2, 2, 2, 2
        ));
        test_policy_tree_.insert(ShapingPolicy(
            POLICY_ID_LOW_PRIO, NO_PARENT, "LowPrio",
            1000000, 1000000, 1500, 1500,
            policy::SchedulingAlgorithm::STRICT_PRIORITY, 100, 0,
            false, 1, 0, 0, 3, 3, 3
        ));
        test_policy_tree_.insert(ShapingPolicy(
            POLICY_ID_DROP_RED_PACKETS, NO_PARENT, "DropRed",
            100000, 100000, 200, 200,
            policy::SchedulingAlgorithm::STRICT_PRIORITY, 100, 0,
            true, 4, 3, 0, 4, 4, 4
        ));
        test_policy_tree_.insert(ShapingPolicy(
            POLICY_ID_DEFAULT, NO_PARENT, "Default",
            10000000, 10000000, 15000, 15000,
            policy::SchedulingAlgorithm::STRICT_PRIORITY, 100, 0,
            false, 0, 0, 0, 5, 5, 5
        ));


        classifier_ = std::make_unique<dataplane::FlowClassifier>(test_flow_table_, POLICY_ID_DEFAULT);
        shaper_ = std::make_unique<TrafficShaper>(test_policy_tree_, *classifier_, test_flow_table_);

        size_t num_sps_levels = 8; // For priorities 0-7
        main_scheduler_ = std::make_unique<scheduler::StrictPriorityScheduler>(createPermissiveSpsParams(num_sps_levels));

        pipeline_ = std::make_unique<PacketPipeline>(*classifier_, *shaper_, *main_scheduler_);
    }

    void set_policy_for_flow_tuple(const dataplane::FiveTuple& five_tuple, policy::PolicyId policy_id) {
        core::FlowId flow_id = classifier_->get_or_create_flow(five_tuple);
        ASSERT_TRUE(test_flow_table_.count(flow_id));
        test_flow_table_.at(flow_id).policy_id = policy_id;
    }
};


TEST_F(PacketPipelineTest, HandleSingleGreenPacket) {
    dataplane::FiveTuple tuple1(1, 1, 100, 200, 6);
    set_policy_for_flow_tuple(tuple1, POLICY_ID_HIGH_PRIO);

    pipeline_->handle_incoming_packet(tuple1, 100);

    scheduler::PacketDescriptor packet_out = pipeline_->get_next_packet_to_transmit();

    ASSERT_NE(packet_out.packet_length_bytes, 0);
    ASSERT_EQ(packet_out.priority, 7);
    ASSERT_EQ(packet_out.conformance, scheduler::ConformanceLevel::GREEN);
    ASSERT_EQ(packet_out.flow_id, classifier_->get_or_create_flow(tuple1));


    scheduler::PacketDescriptor empty_packet = pipeline_->get_next_packet_to_transmit();
    ASSERT_EQ(empty_packet.packet_length_bytes, 0);
}

TEST_F(PacketPipelineTest, PacketDroppedByShaperPolicy) {
    dataplane::FiveTuple tuple_drop(2, 2, 100, 200, 6);
    set_policy_for_flow_tuple(tuple_drop, POLICY_ID_DROP_RED_PACKETS);

    // Policy 4: CIR/PIR 100k bps (12500 Bps), CBS/PBS 200B. drop_on_red=true
    // P1 (150B): GREEN. CIR: 200-150=50. PIR: 200-150=50.
    pipeline_->handle_incoming_packet(tuple_drop, 150);

    // P2 (150B): CIR Fails (needs 150, has 50). PIR: Fails (needs 150, has 50). RED.
    // Policy drops RED. So this packet should not be enqueued.
    pipeline_->handle_incoming_packet(tuple_drop, 150);

    // P3 (10B): Should also be RED and dropped, as buckets are still empty.
    pipeline_->handle_incoming_packet(tuple_drop, 10);

    scheduler::PacketDescriptor packet_out1 = pipeline_->get_next_packet_to_transmit();
    ASSERT_NE(packet_out1.packet_length_bytes, 0);
    ASSERT_EQ(packet_out1.flow_id, classifier_->get_or_create_flow(tuple_drop));
    ASSERT_EQ(packet_out1.conformance, scheduler::ConformanceLevel::GREEN); // P1 was GREEN

    // Only P1 should have been enqueued.
    scheduler::PacketDescriptor packet_out_empty = pipeline_->get_next_packet_to_transmit();
    ASSERT_EQ(packet_out_empty.packet_length_bytes, 0);
}


TEST_F(PacketPipelineTest, PriorityOrderThroughPipeline) {
    dataplane::FiveTuple tuple_high(1,1,1,1,6);
    dataplane::FiveTuple tuple_mid(2,2,2,2,6);
    dataplane::FiveTuple tuple_low(3,3,3,3,6);

    set_policy_for_flow_tuple(tuple_high, POLICY_ID_HIGH_PRIO); // Prio 7 for GREEN
    set_policy_for_flow_tuple(tuple_mid, POLICY_ID_MID_PRIO);   // Prio 4 for GREEN
    set_policy_for_flow_tuple(tuple_low, POLICY_ID_LOW_PRIO);   // Prio 1 for GREEN

    // Enqueue in mixed up order (low, high, mid) - all should be GREEN
    pipeline_->handle_incoming_packet(tuple_low, 100);
    pipeline_->handle_incoming_packet(tuple_high, 100);
    pipeline_->handle_incoming_packet(tuple_mid, 100);

    scheduler::PacketDescriptor p_out_high = pipeline_->get_next_packet_to_transmit();
    ASSERT_NE(p_out_high.packet_length_bytes, 0);
    ASSERT_EQ(p_out_high.priority, 7);
    ASSERT_EQ(p_out_high.flow_id, classifier_->get_or_create_flow(tuple_high));

    scheduler::PacketDescriptor p_out_mid = pipeline_->get_next_packet_to_transmit();
    ASSERT_NE(p_out_mid.packet_length_bytes, 0);
    ASSERT_EQ(p_out_mid.priority, 4);
    ASSERT_EQ(p_out_mid.flow_id, classifier_->get_or_create_flow(tuple_mid));

    scheduler::PacketDescriptor p_out_low = pipeline_->get_next_packet_to_transmit();
    ASSERT_NE(p_out_low.packet_length_bytes, 0);
    ASSERT_EQ(p_out_low.priority, 1);
    ASSERT_EQ(p_out_low.flow_id, classifier_->get_or_create_flow(tuple_low));

    ASSERT_TRUE(main_scheduler_->is_empty());
}

TEST_F(PacketPipelineTest, NewFlowGetsDefaultPolicy) {
    dataplane::FiveTuple tuple_new(10,10,10,10,6); // This tuple hasn't been set explicitly

    pipeline_->handle_incoming_packet(tuple_new, 100); // Should get POLICY_ID_DEFAULT

    scheduler::PacketDescriptor packet_out = pipeline_->get_next_packet_to_transmit();
    ASSERT_NE(packet_out.packet_length_bytes, 0);
    ASSERT_EQ(packet_out.flow_id, classifier_->get_or_create_flow(tuple_new));
    ASSERT_EQ(packet_out.priority, 0); // Default policy gives priority 0 for GREEN
    ASSERT_EQ(packet_out.conformance, scheduler::ConformanceLevel::GREEN);
}

} // namespace core
} // namespace hqts
