#include "hqts/dataplane/flow_classifier.h"
#include <stdexcept> // For std::runtime_error (though not used in current version)
#include <string>    // For std::to_string (not used in current version)

namespace hqts {
namespace dataplane {

FlowClassifier::FlowClassifier(core::FlowTable& ft, policy::PolicyId default_pid)
    : flow_table_(ft),
      default_policy_id_(default_pid),
      next_flow_id_(1) { // Start FlowIds from 1 (0 might be reserved)

    // A check could be added here to ensure default_policy_id_ is valid,
    // but FlowClassifier doesn't have direct access to PolicyTree to verify.
    // This responsibility would typically lie with the system initializing the FlowClassifier.
    // For example, if policy_id 0 is invalid unless it's a special NO_POLICY_ID marker.
    // if (default_policy_id_ == 0 && default_policy_id_ != policy::NO_PARENT_POLICY_ID) { // Example check
    //     throw std::invalid_argument("FlowClassifier: Invalid default_policy_id provided.");
    // }
}

core::FlowId FlowClassifier::get_or_create_flow(const FiveTuple& five_tuple) {
    std::lock_guard<std::mutex> lock(mutex_); // Thread-safe access to shared members

    // Check if FlowKey (FiveTuple) already exists in our map
    auto it = flow_key_to_flow_id_map_.find(five_tuple);
    if (it != flow_key_to_flow_id_map_.end()) {
        // Found existing flow, return its FlowId
        return it->second;
    }

    // New flow detected
    core::FlowId new_id = next_flow_id_++;
    flow_key_to_flow_id_map_[five_tuple] = new_id; // Store mapping from FiveTuple to new FlowId

    // Create a new FlowContext for this flow and add it to the global FlowTable.
    // The FlowContext constructor needs: FlowId, PolicyId, QueueId, DropPolicy.
    // For a new flow, we use the default_policy_id_.
    // Default QueueId and DropPolicy need to be defined. These might ideally come from
    // inspecting the default_policy_id_ in the PolicyTree, or be system-wide defaults.
    // For this implementation, we'll use simple placeholder defaults.
    core::QueueId default_initial_queue_id = 0; // Placeholder - might be derived from policy
    core::DropPolicy default_initial_drop_policy = core::DropPolicy::TAIL_DROP; // Placeholder

    core::FlowContext new_flow_context(new_id, default_policy_id_,
                                       default_initial_queue_id, default_initial_drop_policy);

    // Optionally, store the FlowKey (FiveTuple) in FlowContext if FlowContext is extended to hold it.
    // Example: new_flow_context.flow_key = five_tuple;

    auto emplace_result = flow_table_.emplace(new_id, new_flow_context);
    if (!emplace_result.second) {
        // This would be highly unusual if next_flow_id_ is strictly monotonic and unique.
        // It implies a FlowId collision in the global flow_table_, which should not happen.
        // Handle error: throw, log, or overwrite. For now, let's assume this won't fail.
        // (Could happen if FlowTable keys are not managed solely by this classifier's next_flow_id_)
    }

    return new_id;
}

// Placeholder for PacketDescriptor overload - commented out in header
/*
core::FlowId FlowClassifier::get_or_create_flow(const scheduler::PacketDescriptor& packet) {
    // 1. Extract FiveTuple from packet (requires packet parsing logic or richer PacketDescriptor)
    //    FiveTuple five_tuple = ... ;
    // 2. Call the primary get_or_create_flow
    //    return get_or_create_flow(five_tuple);

    throw std::logic_error("get_or_create_flow from PacketDescriptor is not yet implemented.");
}
*/

} // namespace dataplane
} // namespace hqts
