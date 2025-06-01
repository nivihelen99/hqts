#ifndef HQTS_CORE_TRAFFIC_SHAPER_H_
#define HQTS_CORE_TRAFFIC_SHAPER_H_

#include "hqts/core/flow_context.h"
#include "hqts/core/shaping_policy.h" // Includes TokenBucket
#include "hqts/policy/policy_tree.h"  // For PolicyTree
#include "hqts/scheduler/packet_descriptor.h"
// #include "hqts/scheduler/scheduler_interface.h" // Not directly used in this phase of TrafficShaper

#include <memory> // For std::shared_ptr or std::reference_wrapper (not strictly needed with references)
#include <map>    // For potential scheduler_map_ if used later

namespace hqts {
namespace core {

class TrafficShaper {
public:
    /**
     * @brief Constructor for TrafficShaper.
     * @param policy_tree A non-const reference to the policy tree, as token bucket states will be modified.
     */
    explicit TrafficShaper(policy::PolicyTree& policy_tree);

    // TrafficShaper is likely unique and stateful, manage copying carefully if needed.
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
     * @param packet The packet descriptor to process. Modified by reference.
     * @param flow_context The flow context providing the policy_id for this packet.
     * @return True if the packet is to be enqueued, false if it should be dropped.
     * @throws std::runtime_error if no policy is found for the flow's policy_id.
     */
    bool process_packet(scheduler::PacketDescriptor& packet, const FlowContext& flow_context);

private:
    policy::PolicyTree& policy_tree_; // Non-const reference to allow modification of policies (token buckets)

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
