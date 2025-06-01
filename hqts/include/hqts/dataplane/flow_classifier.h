#ifndef HQTS_DATAPLANE_FLOW_CLASSIFIER_H_
#define HQTS_DATAPLANE_FLOW_CLASSIFIER_H_

#include "hqts/dataplane/flow_identifier.h" // For FiveTuple, FlowKey
#include "hqts/policy/policy_types.h"      // For policy::PolicyId
#include <cstdint>                         // For std::uint64_t
#include <unordered_map>
#include <mutex>    // For std::mutex and std::lock_guard
#include <memory>   // For std::shared_ptr (not used here, but often in similar contexts)

// Forward declaration for FlowTable
namespace hqts { namespace core { class FlowTable; } }

// Ensure core::FlowId is defined for use in this header
// (Matches the definition in hqts/core/flow_context.h)
namespace hqts { namespace core { using FlowId = std::uint64_t; } }

namespace hqts {
namespace dataplane {

class FlowClassifier {
public:
    /**
     * @brief Constructor for FlowClassifier.
     * @param flow_table A reference to the application's main FlowTable where FlowContexts are stored.
     * @param default_policy_id The PolicyId to assign to newly identified flows.
     */
    FlowClassifier(core::FlowTable& flow_table, policy::PolicyId default_policy_id);

    // FlowClassifier might be stateful and potentially shared, so manage copy/move carefully.
    // For now, make it non-copyable and non-movable to simplify.
    FlowClassifier(const FlowClassifier&) = delete;
    FlowClassifier& operator=(const FlowClassifier&) = delete;
    FlowClassifier(FlowClassifier&&) = delete;
    FlowClassifier& operator=(FlowClassifier&&) = delete;

    /**
     * @brief Gets the FlowId for a given FiveTuple.
     * If the flow is new, it creates a FlowId, adds a corresponding FlowContext
     * to the FlowTable using the default_policy_id, and maps the FiveTuple to the new FlowId.
     * @param five_tuple The 5-tuple identifying the flow.
     * @return The core::FlowId associated with this flow.
     */
    core::FlowId get_or_create_flow(const FiveTuple& five_tuple);

    // Convenience overload for PacketDescriptor might be added later if PacketDescriptor is enhanced
    // to easily provide a FiveTuple or if parsing logic is integrated here.
    // core::FlowId get_or_create_flow(const scheduler::PacketDescriptor& packet);

private:
    core::FlowTable& flow_table_; // Reference to the global flow table (stores FlowContext)

    // Internal map to quickly find an existing FlowId for a given FlowKey (FiveTuple)
    std::unordered_map<FlowKey, core::FlowId> flow_key_to_flow_id_map_;

    core::FlowId next_flow_id_ = 1; // Monotonically increasing counter for new FlowIds
    policy::PolicyId default_policy_id_; // Policy to assign to new flows

    std::mutex mutex_; // Mutex to protect access to flow_key_to_flow_id_map_ and next_flow_id_
};

} // namespace dataplane
} // namespace hqts

#endif // HQTS_DATAPLANE_FLOW_CLASSIFIER_H_
