#include "gtest/gtest.h"
#include "hqts/dataplane/flow_table.h" // Includes hqts/core/flow_context.h
#include "hqts/core/flow_context.h"    // For direct use of core types like FlowId, PolicyId etc.
#include "hqts/policy/policy_types.h"  // For policy::PolicyId if needed (though flow_context includes it)
#include <string>                      // Not strictly needed for FlowId=uint64_t but good for general context

namespace hqts {
namespace dataplane {

// Helper to create a FlowContext for tests
// Uses types from hqts::core
core::FlowContext createTestFlowContext(core::FlowId id, policy::PolicyId policy_id = 1, core::QueueId queue_id = 0, core::DropPolicy dp = core::DropPolicy::TAIL_DROP) {
    // The FlowContext constructor defined previously will set initial timestamps etc.
    return core::FlowContext(id, policy_id, queue_id, dp);
}

class FlowTableTest : public ::testing::Test {
protected:
    core::FlowTable flow_table_; // Instance of std::unordered_map<core::FlowId, core::FlowContext>
};

TEST_F(FlowTableTest, EmptyTable) {
    ASSERT_TRUE(flow_table_.empty());
    ASSERT_EQ(flow_table_.size(), 0);
}

TEST_F(FlowTableTest, AddAndFindFlow) {
    core::FlowId id1 = 12345;
    core::FlowContext context1 = createTestFlowContext(id1, 101);

    // Add using operator[]
    flow_table_[id1] = context1;
    ASSERT_EQ(flow_table_.size(), 1);
    ASSERT_FALSE(flow_table_.empty());

    // Find using find()
    auto it = flow_table_.find(id1);
    ASSERT_NE(it, flow_table_.end());
    ASSERT_EQ(it->first, id1); // Key is FlowId
    ASSERT_EQ(it->second.flow_id, id1); // Check context's own ID
    ASSERT_EQ(it->second.policy_id, 101);

    // Find non-existent flow
    core::FlowId id_non_existent = 99999;
    auto it_non_existent = flow_table_.find(id_non_existent);
    ASSERT_EQ(it_non_existent, flow_table_.end());
}

TEST_F(FlowTableTest, AddMultipleFlows) {
    core::FlowId id1 = 1;
    core::FlowId id2 = 2;
    flow_table_[id1] = createTestFlowContext(id1);
    flow_table_[id2] = createTestFlowContext(id2, 202);

    ASSERT_EQ(flow_table_.size(), 2);

    auto it1 = flow_table_.find(id1);
    ASSERT_NE(it1, flow_table_.end());
    ASSERT_EQ(it1->second.flow_id, id1);

    auto it2 = flow_table_.find(id2);
    ASSERT_NE(it2, flow_table_.end());
    ASSERT_EQ(it2->second.flow_id, id2);
    ASSERT_EQ(it2->second.policy_id, 202);
}

TEST_F(FlowTableTest, UpdateFlowContext) {
    core::FlowId id1 = 100;
    flow_table_[id1] = createTestFlowContext(id1, 10, 1, core::DropPolicy::TAIL_DROP);

    // Retrieve and modify
    auto it = flow_table_.find(id1);
    ASSERT_NE(it, flow_table_.end());

    it->second.policy_id = 20; // Modify policy_id
    it->second.current_rate_bps = 500000; // Modify some state
    // No need to re-insert for std::unordered_map if modifying value in place via iterator

    // Verify update
    auto it_updated = flow_table_.find(id1);
    ASSERT_NE(it_updated, flow_table_.end());
    ASSERT_EQ(it_updated->second.policy_id, 20);
    ASSERT_EQ(it_updated->second.current_rate_bps, 500000);

    // Alternative: replace object
    core::FlowContext new_context = createTestFlowContext(id1, 30, 2, core::DropPolicy::RED);
    flow_table_[id1] = new_context; // Replaces existing or inserts if not present

    it_updated = flow_table_.find(id1);
    ASSERT_NE(it_updated, flow_table_.end());
    ASSERT_EQ(it_updated->second.policy_id, 30);
    ASSERT_EQ(it_updated->second.queue_id, 2);
    ASSERT_EQ(it_updated->second.drop_policy, core::DropPolicy::RED);
}

TEST_F(FlowTableTest, EraseFlow) {
    core::FlowId id1 = 1;
    core::FlowId id2 = 2;
    flow_table_[id1] = createTestFlowContext(id1);
    flow_table_[id2] = createTestFlowContext(id2);
    ASSERT_EQ(flow_table_.size(), 2);

    // Erase by key
    size_t erased_count = flow_table_.erase(id1);
    ASSERT_EQ(erased_count, 1);
    ASSERT_EQ(flow_table_.size(), 1);
    ASSERT_EQ(flow_table_.find(id1), flow_table_.end()); // Verify it's gone
    ASSERT_NE(flow_table_.find(id2), flow_table_.end()); // Verify other is still there

    // Try erasing non-existent key
    erased_count = flow_table_.erase(999);
    ASSERT_EQ(erased_count, 0);
    ASSERT_EQ(flow_table_.size(), 1);
}

TEST_F(FlowTableTest, AddUsingInsert) {
    core::FlowId id1 = 777;
    core::FlowContext context1 = createTestFlowContext(id1, 303);

    // Using insert method
    // std::unordered_map::insert returns std::pair<iterator, bool>
    auto result = flow_table_.insert({id1, context1});
    ASSERT_TRUE(result.second); // bool part of pair is true if insertion took place
    ASSERT_EQ(flow_table_.size(), 1);
    ASSERT_NE(result.first, flow_table_.end()); // iterator should be valid
    ASSERT_EQ(result.first->second.policy_id, 303); // iterator points to inserted element

    // Try inserting duplicate FlowId
    core::FlowContext context2_dup = createTestFlowContext(id1, 404);
    auto result_dup = flow_table_.insert({id1, context2_dup});
    ASSERT_FALSE(result_dup.second); // Insertion should not happen for duplicate key
    ASSERT_EQ(flow_table_.size(), 1); // Size should remain 1
    ASSERT_NE(result_dup.first, flow_table_.end()); // iterator should point to existing element
    ASSERT_EQ(result_dup.first->second.policy_id, 303); // Check that it's the original element
}

// If FlowId were a complex struct, specific tests for hash/equality functionality
// in the context of std::unordered_map would be important.
// For FlowId = uint64_t, these are implicitly handled by the standard library's
// std::hash<uint64_t> and operator== for uint64_t.

} // namespace dataplane
} // namespace hqts
