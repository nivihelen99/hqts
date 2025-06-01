#include "gtest/gtest.h"
#include "hqts/policy/policy_tree.h" // Includes shaping_policy.h -> token_bucket.h, policy_types.h
#include "hqts/policy/policy_types.h" // For NO_PARENT_POLICY_ID, SchedulingAlgorithm etc.
#include <string>
#include <vector>
#include <algorithm> // For std::find
#include <iostream>  // For potential debug prints (though not used in final assertions)

// Note: hqts/core/shaping_policy.h and hqts/core/token_bucket.h are implicitly included
// via hqts/policy/policy_tree.h

namespace hqts {
namespace policy {

// Helper to create a ShapingPolicy with default token bucket params for simplicity in tests
// Adjust rates/capacities as needed if specific TB behavior is tested via PolicyTree
core::ShapingPolicy createTestPolicy(
    PolicyId id, PolicyId parent_id, const std::string& name,
    Priority priority = 0, SchedulingAlgorithm algo = SchedulingAlgorithm::WFQ,
    uint64_t cir = 1000000, uint64_t cbs = 1500, // 1 Mbps, 1500 Bytes burst
    uint64_t pir = 2000000, uint64_t pbs = 3000, // 2 Mbps, 3000 Bytes burst
    uint32_t weight = 100) {
    return core::ShapingPolicy(
        id, parent_id, name,
        cir, pir, cbs, pbs,
        algo, weight, priority
    );
}

class PolicyTreeTest : public ::testing::Test {
protected:
    PolicyTree tree;

    // Helper to add a policy.
    // For PolicyTree unit tests, we focus on the tree's direct operations.
    // More complex parent-child linking logic would be in a PolicyManager.
    void addPolicyToTree(const core::ShapingPolicy& policy) {
        auto result = tree.insert(policy);
        // ASSERT_TRUE(result.second) could fail if a policy with the same ID is re-added
        // For tests that expect successful insertion, this is fine.
        // For tests testing duplicate prevention, the result.second is checked directly.
        if (!result.second) {
            // Optionally print a warning or handle if tests are not expecting this.
            // std::cerr << "Warning: Policy ID " << policy.id << " might already exist." << std::endl;
        }
    }
};

TEST_F(PolicyTreeTest, EmptyTree) {
    ASSERT_TRUE(tree.empty());
    ASSERT_EQ(tree.size(), 0);
}

TEST_F(PolicyTreeTest, InsertAndFindById) {
    core::ShapingPolicy p1 = createTestPolicy(1, NO_PARENT_POLICY_ID, "root1");
    addPolicyToTree(p1);
    ASSERT_EQ(tree.size(), 1);
    ASSERT_FALSE(tree.empty());

    auto& id_index = tree.get<by_id>();
    auto it = id_index.find(1);
    ASSERT_NE(it, id_index.end());
    ASSERT_EQ(it->id, 1);
    ASSERT_EQ(it->name, "root1");

    auto it_non_existent = id_index.find(999);
    ASSERT_EQ(it_non_existent, id_index.end());
}

TEST_F(PolicyTreeTest, InsertMultiplePolicies) {
    addPolicyToTree(createTestPolicy(1, NO_PARENT_POLICY_ID, "root1"));
    addPolicyToTree(createTestPolicy(2, 1, "child1.1", 1)); // Priority 1
    addPolicyToTree(createTestPolicy(3, 1, "child1.2", 0)); // Priority 0
    addPolicyToTree(createTestPolicy(4, NO_PARENT_POLICY_ID, "root2", 0));
    ASSERT_EQ(tree.size(), 4);
}

TEST_F(PolicyTreeTest, FindByParentId) {
    addPolicyToTree(createTestPolicy(1, NO_PARENT_POLICY_ID, "root1"));
    addPolicyToTree(createTestPolicy(2, 1, "child1.1"));
    addPolicyToTree(createTestPolicy(3, 1, "child1.2"));
    addPolicyToTree(createTestPolicy(4, 2, "grandchild2.1"));
    addPolicyToTree(createTestPolicy(5, NO_PARENT_POLICY_ID, "root2"));


    auto& parent_index = tree.get<by_parent_id>();
    // Children of policy 1
    auto range_children_of_1 = parent_index.equal_range(1);
    int count_children_of_1 = 0;
    std::vector<PolicyId> children1_ids;
    for (auto it = range_children_of_1.first; it != range_children_of_1.second; ++it) {
        count_children_of_1++;
        children1_ids.push_back(it->id);
    }
    ASSERT_EQ(count_children_of_1, 2);
    ASSERT_NE(std::find(children1_ids.begin(), children1_ids.end(), 2), children1_ids.end());
    ASSERT_NE(std::find(children1_ids.begin(), children1_ids.end(), 3), children1_ids.end());

    // Children of policy 3 (should be none)
    auto range_no_children = parent_index.equal_range(3);
    ASSERT_EQ(std::distance(range_no_children.first, range_no_children.second), 0);

    // Root policies (children of NO_PARENT_POLICY_ID)
    auto range_root_children = parent_index.equal_range(NO_PARENT_POLICY_ID);
    int count_root_policies = 0;
    std::vector<PolicyId> root_policy_ids;
    for (auto it = range_root_children.first; it != range_root_children.second; ++it) {
        count_root_policies++;
        root_policy_ids.push_back(it->id);
    }
    ASSERT_EQ(count_root_policies, 2);
    ASSERT_NE(std::find(root_policy_ids.begin(), root_policy_ids.end(), 1), root_policy_ids.end());
    ASSERT_NE(std::find(root_policy_ids.begin(), root_policy_ids.end(), 5), root_policy_ids.end());
}

TEST_F(PolicyTreeTest, FindByPriority) {
    addPolicyToTree(createTestPolicy(1, NO_PARENT_POLICY_ID, "p_high", 0)); // Priority 0
    addPolicyToTree(createTestPolicy(2, NO_PARENT_POLICY_ID, "p_low", 5));  // Priority 5
    addPolicyToTree(createTestPolicy(3, 1, "p_high_child", 0)); // Priority 0

    auto& priority_index = tree.get<by_priority_level>();
    auto range = priority_index.equal_range(0); // Policies with priority 0
    int count = 0;
    std::vector<PolicyId> priority0_ids;
    for (auto it = range.first; it != range.second; ++it) {
        count++;
        priority0_ids.push_back(it->id);
    }
    ASSERT_EQ(count, 2);
    ASSERT_NE(std::find(priority0_ids.begin(), priority0_ids.end(), 1), priority0_ids.end());
    ASSERT_NE(std::find(priority0_ids.begin(), priority0_ids.end(), 3), priority0_ids.end());

    auto range_prio5 = priority_index.equal_range(5);
    ASSERT_EQ(std::distance(range_prio5.first, range_prio5.second), 1);
    ASSERT_EQ(range_prio5.first->id, 2);
}

TEST_F(PolicyTreeTest, FindByName) {
    addPolicyToTree(createTestPolicy(1, NO_PARENT_POLICY_ID, "unique_name"));
    addPolicyToTree(createTestPolicy(2, 1, "shared_name"));
    addPolicyToTree(createTestPolicy(3, 1, "shared_name"));

    auto& name_index = tree.get<by_name>();
    auto it_unique = name_index.find("unique_name");
    ASSERT_NE(it_unique, name_index.end());
    ASSERT_EQ(it_unique->id, 1);

    auto range_shared = name_index.equal_range("shared_name");
    ASSERT_EQ(std::distance(range_shared.first, range_shared.second), 2);
    // Could further check IDs if necessary
}

TEST_F(PolicyTreeTest, ModifyPolicy) {
    core::ShapingPolicy p1 = createTestPolicy(1, NO_PARENT_POLICY_ID, "original_name");
    addPolicyToTree(p1);

    auto& id_index = tree.get<by_id>(); // Get the id_index
    auto it = id_index.find(1);
    ASSERT_NE(it, id_index.end());

    bool modified = tree.modify(it, [](core::ShapingPolicy& policy) {
        policy.name = "modified_name";
        policy.weight = 200;
    });
    ASSERT_TRUE(modified);

    // Re-fetch to check (iterator 'it' is still valid after modify)
    ASSERT_EQ(it->name, "modified_name");
    ASSERT_EQ(it->weight, 200);
}

TEST_F(PolicyTreeTest, ModifyPolicyAffectingIndexedField) {
    core::ShapingPolicy p1 = createTestPolicy(1, NO_PARENT_POLICY_ID, "name1", 0); // Priority 0
    addPolicyToTree(p1);
    addPolicyToTree(createTestPolicy(2, NO_PARENT_POLICY_ID, "name2", 1)); // Priority 1

    auto& id_index = tree.get<by_id>();
    auto it_p1 = id_index.find(1);
    ASSERT_NE(it_p1, id_index.end());

    bool modified = tree.modify(it_p1, [](core::ShapingPolicy& policy) {
        policy.priority_level = 2; // Change from 0 to 2
        policy.name = "name1_modified"; // Also change name
    });
    ASSERT_TRUE(modified);

    // Check old priority index
    auto& priority_index = tree.get<by_priority_level>();
    auto range_prio0 = priority_index.equal_range(0);
    ASSERT_EQ(std::distance(range_prio0.first, range_prio0.second), 0);

    // Check new priority index
    auto range_prio2 = priority_index.equal_range(2);
    ASSERT_EQ(std::distance(range_prio2.first, range_prio2.second), 1);
    ASSERT_EQ(range_prio2.first->id, 1);
    ASSERT_EQ(range_prio2.first->name, "name1_modified");

    // Check name index
    auto& name_index = tree.get<by_name>();
    auto it_name_orig = name_index.find("name1");
    ASSERT_EQ(it_name_orig, name_index.end());
    auto it_name_mod = name_index.find("name1_modified");
    ASSERT_NE(it_name_mod, name_index.end());
    ASSERT_EQ(it_name_mod->id, 1);
}

TEST_F(PolicyTreeTest, ErasePolicy) {
    addPolicyToTree(createTestPolicy(1, NO_PARENT_POLICY_ID, "root1"));
    addPolicyToTree(createTestPolicy(2, 1, "child1.1"));
    ASSERT_EQ(tree.size(), 2);

    auto& id_index = tree.get<by_id>(); // Get the id_index view

    // Erase by iterator from the id_index view
    auto it_child = id_index.find(2);
    ASSERT_NE(it_child, id_index.end());
    tree.erase(it_child); // Erase using iterator from the correct index

    ASSERT_EQ(tree.size(), 1);
    auto it_after_erase = id_index.find(2);
    ASSERT_EQ(it_after_erase, id_index.end());

    // Erase by key from the primary index (id_index)
    size_t erased_count = id_index.erase(1); // Erase root by key
    ASSERT_EQ(erased_count, 1);
    ASSERT_TRUE(tree.empty());
}


TEST_F(PolicyTreeTest, PreventDuplicateId) {
    addPolicyToTree(createTestPolicy(1, NO_PARENT_POLICY_ID, "p1"));
    core::ShapingPolicy p_duplicate = createTestPolicy(1, NO_PARENT_POLICY_ID, "p_dup");

    auto result = tree.insert(p_duplicate);
    ASSERT_FALSE(result.second);
    ASSERT_EQ(tree.size(), 1);
}

TEST_F(PolicyTreeTest, RootPoliciesHaveNoParent) {
    addPolicyToTree(createTestPolicy(1, hqts::policy::NO_PARENT_POLICY_ID, "root1"));
    addPolicyToTree(createTestPolicy(2, 1, "child1"));
    addPolicyToTree(createTestPolicy(3, hqts::policy::NO_PARENT_POLICY_ID, "root2"));

    auto& parent_idx = tree.get<by_parent_id>();
    auto range = parent_idx.equal_range(hqts::policy::NO_PARENT_POLICY_ID);

    int count = 0;
    std::vector<PolicyId> root_ids;
    for(auto it = range.first; it != range.second; ++it) {
        count++;
        root_ids.push_back(it->id);
    }
    ASSERT_EQ(count, 2);
    ASSERT_NE(std::find(root_ids.begin(), root_ids.end(), 1), root_ids.end());
    ASSERT_NE(std::find(root_ids.begin(), root_ids.end(), 3), root_ids.end());
}

} // namespace policy
} // namespace hqts
