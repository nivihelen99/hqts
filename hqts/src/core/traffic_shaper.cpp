#include "hqts/core/traffic_shaper.h"
// Other necessary direct includes for .cpp specific types if any, were already added/verified.
// flow_classifier.h, flow_identifier.h, flow_context.h, flow_table.h should be
// transitively included via traffic_shaper.h now.
#include <stdexcept> // For std::runtime_error
#include <string>    // For std::to_string in error messages

namespace hqts {
namespace core {

TrafficShaper::TrafficShaper(policy::PolicyTree& pt, dataplane::FlowClassifier& fc, core::FlowTable& ft)
    : policy_tree_(pt), flow_classifier_(fc), flow_table_(ft) {
    // Constructor body
}

// Private helper method implementation
scheduler::ConformanceLevel TrafficShaper::apply_token_buckets(
    const scheduler::PacketDescriptor& packet,
    ShapingPolicy& policy) { // Policy is non-const as its token buckets are modified

    // TokenBucket::consume is const but modifies mutable members.
    bool conforms_to_cir = policy.cir_bucket.consume(packet.packet_length_bytes);

    if (conforms_to_cir) {
        // If it conforms to CIR, it's GREEN.
        // For srTCM-like behavior, green packets also consume from PIR bucket.
        // If PIR bucket is only for yellow, this consume might be conditional or different.
        // Assuming for now that PIR bucket tracks all traffic that has passed CIR.
        policy.pir_bucket.consume(packet.packet_length_bytes);
        return scheduler::ConformanceLevel::GREEN;
    } else {
        // Failed CIR, try PIR for YELLOW
        bool conforms_to_pir = policy.pir_bucket.consume(packet.packet_length_bytes);
        if (conforms_to_pir) {
            return scheduler::ConformanceLevel::YELLOW;
        } else {
            return scheduler::ConformanceLevel::RED;
        }
    }
}

bool TrafficShaper::process_packet(
    scheduler::PacketDescriptor& packet, // Packet is modified (flow_id, conformance, priority)
    const dataplane::FiveTuple& five_tuple) {

    // 1. Classify and get FlowId, ensuring FlowContext exists
    core::FlowId flow_id = flow_classifier_.get_or_create_flow(five_tuple);
    packet.flow_id = flow_id; // Set the flow_id on the packet

    // Retrieve the FlowContext
    auto fc_it = flow_table_.find(flow_id);
    if (fc_it == flow_table_.end()) {
        // This should ideally not happen if get_or_create_flow ensures context creation.
        throw std::runtime_error("TrafficShaper: FlowContext not found in table for flow_id: " + std::to_string(flow_id));
    }
    const core::FlowContext& flow_context = fc_it->second;

    // 2. Retrieve ShapingPolicy for the flow using policy_id from FlowContext
    auto& policy_id_index = policy_tree_.get<policy::by_id>();
    auto policy_it = policy_id_index.find(flow_context.policy_id);

    if (policy_it == policy_id_index.end()) {
        packet.conformance = scheduler::ConformanceLevel::RED;
        // Consider logging this event: Policy ID from FlowContext not found in PolicyTree.
        return false; // Drop if policy referenced by flow context is not found
    }

    // Variables to be set by the modifier lambda (which modifies policy's token buckets)
    scheduler::ConformanceLevel conformance_level;
    bool drop_this_packet = false;

    // Use policy_tree_.modify to get non-const access to the policy object
    // and update its token buckets.
    bool modified_successfully = policy_tree_.modify(policy_it,
        [&](ShapingPolicy& modifiable_policy) { // modifiable_policy is non-const

        conformance_level = this->apply_token_buckets(packet, modifiable_policy); // packet is const here
        packet.conformance = conformance_level; // Modify the packet passed by reference to process_packet

        if (conformance_level == scheduler::ConformanceLevel::RED && modifiable_policy.drop_on_red) {
            drop_this_packet = true;
        } else {
            drop_this_packet = false;
            // Set packet priority based on conformance and policy targets
            switch (conformance_level) {
                case scheduler::ConformanceLevel::GREEN:
                    packet.priority = modifiable_policy.target_priority_green;
                    break;
                case scheduler::ConformanceLevel::YELLOW:
                    packet.priority = modifiable_policy.target_priority_yellow;
                    break;
                case scheduler::ConformanceLevel::RED: // Not dropped by policy.drop_on_red
                    packet.priority = modifiable_policy.target_priority_red;
                    break;
            }
        }
    });

    if (!modified_successfully) {
        // This implies policy_it was invalid (e.g. points to end() or was erased concurrently),
        // though the find() check above should prevent this for valid policy_id.
        throw std::runtime_error("TrafficShaper: Failed to modify policy tree for policy_id: " +
                                 std::to_string(flow_context.policy_id) +
                                 ". Policy iterator may be invalid.");
    }

    return !drop_this_packet;
}

} // namespace core
} // namespace hqts
