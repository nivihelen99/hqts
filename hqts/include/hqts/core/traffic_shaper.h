#ifndef HQTS_CORE_TRAFFIC_SHAPER_H_
#define HQTS_CORE_TRAFFIC_SHAPER_H_

// #include "hqts/core/flow_context.h"        // No longer needed here directly
#include "hqts/core/shaping_policy.h"      // Includes TokenBucket
#include "hqts/policy/policy_tree.h"       // For PolicyTree
#include "hqts/scheduler/packet_descriptor.h"// For PacketDescriptor
#include "hqts/dataplane/flow_classifier.h"// For FlowClassifier
#include "hqts/dataplane/flow_identifier.h"// For FiveTuple
#include "hqts/dataplane/flow_table.h"     // For core::FlowTable definition

#include <memory>
#include <map>    // For potential scheduler_map_ if used later

// No forward declaration for core::FlowTable needed as it's now included.

namespace hqts {
namespace core {

class TrafficShaper {
public:
    /**
     * @brief Constructor for TrafficShaper.
     * @param policy_tree A non-const reference to the policy tree for policy lookups.
     * @param flow_classifier A reference to the flow classifier for FlowId retrieval/creation.
     * @param flow_table A reference to the flow table for accessing FlowContext.
     */
    explicit TrafficShaper(policy::PolicyTree& policy_tree,
                           dataplane::FlowClassifier& flow_classifier,
                           core::FlowTable& flow_table);

    // TrafficShaper is stateful (via references) and unique in its role.
    TrafficShaper(const TrafficShaper&) = delete;
    TrafficShaper& operator=(const TrafficShaper&) = delete;
    TrafficShaper(TrafficShaper&&) = default;
    TrafficShaper& operator=(TrafficShaper&&) = default;

    /**
     * @brief Processes a packet against its flow's shaping policy.
     *
     * This method applies token buckets from the policy to the packet, determines its
     * conformance level (GREEN, YELLOW, RED), sets this conformance on the packet,
     * and updates the packet's priority based on the policy's target priorities for that
     * conformance level.
     *
     * It returns true if the packet should be enqueued by the caller (i.e., not dropped
     * by the shaper based on policy rules like 'drop_on_red'). False means the shaper
     * determined the packet should be dropped.
     *
     * @param packet The packet descriptor to process. Modified by reference (flow_id, conformance, priority).
     * @param five_tuple The 5-tuple used to classify the packet and find/create its flow context.
     * @return True if the packet is to be enqueued, false if it should be dropped.
     * @throws std::runtime_error if policy or flow context issues occur.
     */
    bool process_packet(scheduler::PacketDescriptor& packet, const dataplane::FiveTuple& five_tuple);

private:
    policy::PolicyTree& policy_tree_;
    dataplane::FlowClassifier& flow_classifier_;
    core::FlowTable& flow_table_;

    // Example for future scheduler interaction (not used in this subtask):
    // scheduler::SchedulerInterface* target_scheduler_;
    // std::map<core::QueueId, scheduler::SchedulerInterface*> scheduler_map_;

    /**
     * @brief Applies token buckets from the given policy to the packet.
     *
     * Modifies the state of the token buckets within the policy.
     *
     * @param packet The packet descriptor (its length is used).
     * @param policy The ShapingPolicy to apply (non-const, its token buckets will be modified).
     * @return The determined ConformanceLevel for the packet.
     */
    scheduler::ConformanceLevel apply_token_buckets(
        const scheduler::PacketDescriptor& packet, // Packet itself is not changed here, only its length used
        ShapingPolicy& policy                     // Policy's token buckets are modified
    );
};

} // namespace core
} // namespace hqts

#endif // HQTS_CORE_TRAFFIC_SHAPER_H_
