#include "gtest/gtest.h"
#include "hqts/dataplane/flow_classifier.h"
#include "hqts/dataplane/flow_identifier.h" // For FiveTuple
#include "hqts/core/flow_context.h"        // For FlowTable, FlowContext, FlowId
#include "hqts/policy/policy_types.h"      // For PolicyId

#include <thread> // For thread safety tests
#include <vector>
#include <set>    // For checking uniqueness of FlowIds
#include <memory> // For std::unique_ptr

namespace hqts {
namespace dataplane {

class FlowClassifierTest : public ::testing::Test {
protected:
    core::FlowTable test_flow_table_;
    // Using a distinct PolicyId for default to avoid clashes with other test policies if any were global
    static const policy::PolicyId DEFAULT_POLICY_ID = 199;
    std::unique_ptr<FlowClassifier> classifier_;

    void SetUp() override {
        test_flow_table_.clear();
        // A new classifier instance for each test ensures next_flow_id_ starts fresh (from 1).
        classifier_ = std::make_unique<FlowClassifier>(test_flow_table_, DEFAULT_POLICY_ID);
    }

    void TearDown() override {
        // classifier_ unique_ptr will handle cleanup
    }
};

// Define static const member
const policy::PolicyId FlowClassifierTest::DEFAULT_POLICY_ID;

TEST_F(FlowClassifierTest, CreateNewFlow) {
    // Corrected: src_ip, dest_ip, src_port, dest_port, proto
    FiveTuple tuple1(1, 2, 10, 20, 6);

    core::FlowId fid1 = classifier_->get_or_create_flow(tuple1);
    ASSERT_NE(fid1, 0); // Assuming FlowId 0 is invalid or reserved
    ASSERT_EQ(test_flow_table_.count(fid1), 1);

    const core::FlowContext& ctx1 = test_flow_table_.at(fid1);
    ASSERT_EQ(ctx1.flow_id, fid1);
    ASSERT_EQ(ctx1.policy_id, DEFAULT_POLICY_ID);
    // Check default queue_id and drop_policy as set by FlowClassifier's current implementation
    ASSERT_EQ(ctx1.queue_id, 0);
    ASSERT_EQ(ctx1.drop_policy, core::DropPolicy::TAIL_DROP);
}

TEST_F(FlowClassifierTest, GetExistingFlow) {
    // Corrected: src_ip, dest_ip, src_port, dest_port, proto
    FiveTuple tuple1(1, 2, 10, 20, 6);
    core::FlowId fid1_initial = classifier_->get_or_create_flow(tuple1);
    ASSERT_EQ(test_flow_table_.size(), 1);

    core::FlowId fid1_retrieved = classifier_->get_or_create_flow(tuple1);
    ASSERT_EQ(fid1_retrieved, fid1_initial);
    ASSERT_EQ(test_flow_table_.size(), 1); // No new FlowContext should be created
}

TEST_F(FlowClassifierTest, MultipleDistinctFlows) {
    // Corrected: src_ip, dest_ip, src_port, dest_port, proto
    FiveTuple tuple1(101, 102, 10, 20, 6);
    FiveTuple tuple2(201, 202, 30, 40, 17);
    FiveTuple tuple3(301, 302, 100, 200, 6);

    core::FlowId fid1 = classifier_->get_or_create_flow(tuple1);
    core::FlowId fid2 = classifier_->get_or_create_flow(tuple2);
    core::FlowId fid3 = classifier_->get_or_create_flow(tuple3);

    ASSERT_NE(fid1, fid2);
    ASSERT_NE(fid1, fid3);
    ASSERT_NE(fid2, fid3);

    ASSERT_EQ(test_flow_table_.size(), 3);
    ASSERT_TRUE(test_flow_table_.count(fid1));
    ASSERT_TRUE(test_flow_table_.count(fid2));
    ASSERT_TRUE(test_flow_table_.count(fid3));
}

TEST_F(FlowClassifierTest, FlowIdUniqueness) {
    std::vector<FiveTuple> tuples;
    const int num_unique_tuples = 100;
    for (int i = 0; i < num_unique_tuples; ++i) {
        // Corrected: src_ip, dest_ip, src_port, dest_port, proto
        // Create distinct 5-tuples by varying source port
        tuples.emplace_back(1, 2, static_cast<uint16_t>(1000 + i), 80, 6);
    }

    std::set<core::FlowId> generated_flow_ids;
    for (const auto& tuple : tuples) {
        generated_flow_ids.insert(classifier_->get_or_create_flow(tuple));
    }

    ASSERT_EQ(generated_flow_ids.size(), static_cast<size_t>(num_unique_tuples));
    ASSERT_EQ(test_flow_table_.size(), static_cast<size_t>(num_unique_tuples));
}

TEST_F(FlowClassifierTest, ThreadSafety) {
    std::vector<std::thread> threads;
    const int num_threads = 10;
    const int iterations_per_thread = 100;
    std::set<core::FlowId> all_generated_ids;
    std::mutex set_mutex;

    // Corrected: src_ip, dest_ip, src_port, dest_port, proto
    FiveTuple common_tuple(1020, 3040, 50, 60, 6);
    // The first thread to grab the lock for common_tuple will create it. Others will get existing.
    // No need to pre-fetch common_fid_expected before threads, classifier should handle the race.

    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&, i]() { // Capture i by value for unique tuple generation
            std::vector<core::FlowId> local_ids;
            for (int j = 0; j < iterations_per_thread; ++j) {
                if (j % 10 == 0) {
                    local_ids.push_back(classifier_->get_or_create_flow(common_tuple));
                } else {
                    // Corrected: src_ip, dest_ip, src_port, dest_port, proto
                    // Make IPs vary with thread i, ports with loop j for uniqueness
                    FiveTuple unique_tuple(
                        static_cast<uint32_t>(100 + i),
                        static_cast<uint32_t>(200 + i),
                        static_cast<uint16_t>(j),
                        static_cast<uint16_t>(j + 1),
                        17);
                    local_ids.push_back(classifier_->get_or_create_flow(unique_tuple));
                }
            }
            // Lock only when modifying shared set
            std::lock_guard<std::mutex> lock(set_mutex);
            for (core::FlowId id : local_ids) {
                all_generated_ids.insert(id);
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    // Fetch the ID for the common_tuple one last time to confirm it's stable and was created.
    core::FlowId common_fid_final = classifier_->get_or_create_flow(common_tuple);
    ASSERT_TRUE(all_generated_ids.count(common_fid_final));

    // Calculate expected number of unique FlowIds:
    // 1 (for common_tuple) + (num_threads * number of unique tuples generated per thread)
    size_t unique_tuples_per_thread = 0;
     for(int j=0; j < iterations_per_thread; ++j) {
        if (j % 10 != 0) { // This condition identifies when unique tuples are made
            unique_tuples_per_thread++;
        }
     }
    size_t expected_unique_flows = 1 + num_threads * unique_tuples_per_thread;

    ASSERT_EQ(all_generated_ids.size(), expected_unique_flows);
    ASSERT_EQ(test_flow_table_.size(), expected_unique_flows);
}

} // namespace dataplane
} // namespace hqts
