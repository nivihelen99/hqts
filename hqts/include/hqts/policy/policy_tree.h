#ifndef HQTS_POLICY_POLICY_TREE_H_
#define HQTS_POLICY_POLICY_TREE_H_

#include "hqts/core/shaping_policy.h" // Contains ShapingPolicy and PolicyId (via policy_types.h)
// Note: policy_types.h is included by shaping_policy.h, so PolicyId and Priority are available.

#include <boost/multi_index_container.hpp>
#include <boost/multi_index/ordered_index.hpp>
#include <boost/multi_index/member.hpp>
#include <boost/multi_index/tag.hpp>
#include <string> // For std::string in by_name index

namespace hqts {
namespace policy {

// Tags for Boost.MultiIndex container indexing
struct by_id {};
struct by_parent_id {};
struct by_priority_level {}; // For indexing by priority_level field
struct by_name {};         // For indexing by name field

// Define the PolicyTree type using Boost.MultiIndex
using PolicyTree = boost::multi_index::multi_index_container<
    core::ShapingPolicy, // The type of objects stored in the container
    boost::multi_index::indexed_by<
        // Index 0: Unique index by PolicyId (id field)
        boost::multi_index::ordered_unique<
            boost::multi_index::tag<by_id>, // Tag for this index
            boost::multi_index::member<core::ShapingPolicy, policy::PolicyId, &core::ShapingPolicy::id>
        >,
        // Index 1: Non-unique index by parent_id field
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<by_parent_id>, // Tag for this index
            boost::multi_index::member<core::ShapingPolicy, policy::PolicyId, &core::ShapingPolicy::parent_id>
        >,
        // Index 2: Non-unique index by priority_level field
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<by_priority_level>, // Tag for this index
            boost::multi_index::member<core::ShapingPolicy, policy::Priority, &core::ShapingPolicy::priority_level>
        >,
        // Index 3: Non-unique index by name field (optional, useful for lookup by name)
        boost::multi_index::ordered_non_unique<
            boost::multi_index::tag<by_name>, // Tag for this index
            boost::multi_index::member<core::ShapingPolicy, std::string, &core::ShapingPolicy::name>
        >
    >
>;

// Example Usage (Conceptual - actual management class would encapsulate this)
/*
void example_policy_tree_usage() {
    PolicyTree policies;

    // Adding a policy (constructor arguments need to be defined)
    // policies.insert(core::ShapingPolicy(...));

    // Finding a policy by ID
    // auto& id_index = policies.get<by_id>();
    // auto it = id_index.find(some_policy_id);
    // if (it != id_index.end()) {
    //     // Found policy
    // }

    // Finding policies by parent ID
    // auto& parent_id_index = policies.get<by_parent_id>();
    // auto range = parent_id_index.equal_range(a_parent_id);
    // for (auto pit = range.first; pit != range.second; ++pit) {
    //     // Process child policy
    // }
}
*/

} // namespace policy
} // namespace hqts

#endif // HQTS_POLICY_POLICY_TREE_H_
