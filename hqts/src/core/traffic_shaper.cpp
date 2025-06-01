#include "hqts/core/traffic_shaper.h"
#include <stdexcept> // For std::runtime_error
#include <string>    // For std::to_string in error messages

namespace hqts {
namespace core {

TrafficShaper::TrafficShaper(policy::PolicyTree& pt)
    : policy_tree_(pt) {
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
    scheduler::PacketDescriptor& packet, // Packet is modified (conformance, priority)
    const FlowContext& flow_context) {

    // Retrieve the policy for the flow.
    // We need a non-const iterator to pass a non-const ShapingPolicy to the modifier lambda.
    auto& policy_id_index = policy_tree_.get<policy::by_id>();
    auto policy_it = policy_id_index.find(flow_context.policy_id);

    if (policy_it == policy_id_index.end()) {
        // Policy not found for this flow.
        // As per plan, mark RED and let caller decide (or drop if default action).
        // For current design, process_packet indicates drop by returning false.
        packet.conformance = scheduler::ConformanceLevel::RED;
        // packet.priority could be set to a default "drop" priority or lowest if not dropping.
        // For now, if no policy, it's an implicit drop by TrafficShaper.
        // throw std::runtime_error("TrafficShaper: Policy not found for policy_id: " + std::to_string(flow_context.policy_id));
        return false; // Indicate packet should be dropped
    }

    // Variables to be set by the modifier lambda
    scheduler::ConformanceLevel conformance_level;
    bool drop_this_packet = false;

    // Use policy_tree_.modify to get non-const access to the policy object
    // and update its token buckets.
    bool modified_successfully = policy_tree_.modify(policy_it,
        [&](ShapingPolicy& modifiable_policy) {

        conformance_level = this->apply_token_buckets(packet, modifiable_policy);
        packet.conformance = conformance_level;

        if (conformance_level == scheduler::ConformanceLevel::RED && modifiable_policy.drop_on_red) {
            drop_this_packet = true;
        } else {
            drop_this_packet = false;
            // Set packet priority based on conformance and policy targets
            switch (conformance_level) {
                case scheduler::ConformanceLevel::GREEN:
                    packet.priority = modifiable_policy.target_priority_green;
                    // If PacketDescriptor had target_queue_id:
                    // packet.target_queue_id = modifiable_policy.target_queue_id_green;
                    break;
                case scheduler::ConformanceLevel::YELLOW:
                    packet.priority = modifiable_policy.target_priority_yellow;
                    // packet.target_queue_id = modifiable_policy.target_queue_id_yellow;
                    break;
                case scheduler::ConformanceLevel::RED: // Not dropped by policy.drop_on_red
                    packet.priority = modifiable_policy.target_priority_red;
                    // packet.target_queue_id = modifiable_policy.target_queue_id_red;
                    break;
            }
        }
    });

    if (!modified_successfully) {
        // This should ideally not happen if policy_it was valid.
        // It might indicate an issue with the modify operation itself or concurrent modification.
        throw std::runtime_error("TrafficShaper: Failed to modify policy tree for policy_id: " + std::to_string(flow_context.policy_id));
    }

    return !drop_this_packet;
}

} // namespace core
} // namespace hqts
