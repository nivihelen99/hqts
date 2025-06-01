#ifndef HQTS_SCHEDULER_HFSC_SCHEDULER_H_
#define HQTS_SCHEDULER_HFSC_SCHEDULER_H_

#include "hqts/scheduler/scheduler_interface.h"
#include "hqts/scheduler/queue_types.h" // For PacketQueue
#include "hqts/core/flow_context.h"     // For core::FlowId

#include <vector>
#include <map>
#include <chrono>    // For time calculations if needed (e.g. real time for virtual clock updates)
#include <stdexcept> // For std::out_of_range, std::invalid_argument, std::logic_error
#include <string>    // Potentially for error messages
#include <queue>     // For std::priority_queue

namespace hqts {
namespace scheduler {

struct ServiceCurve {
    uint64_t rate_bps;   // Service rate in bits per second (0 means not set or infinite if used as upper limit)
    uint64_t delay_us;   // Initial delay in microseconds (relevant for real-time curve)

    // Default constructor
    ServiceCurve(uint64_t r = 0, uint64_t d = 0) : rate_bps(r), delay_us(d) {}
};

struct HfscFlowState {
    core::FlowId flow_id;
    PacketQueue packet_queue; // Each flow has its own packet queue

    // Service curves
    ServiceCurve real_time_sc;    // For real-time guarantees
    ServiceCurve link_share_sc;   // For fair link sharing
    ServiceCurve upper_limit_sc;  // To cap the service rate

    // HFSC internal state variables (times are typically in a virtual domain)
    uint64_t virtual_start_time = 0;  // Becomes eligible_time based on real-time curve
    uint64_t virtual_finish_time = 0; // Used for link-sharing curve scheduling (or chosen RT/LS VFT)
    uint64_t eligible_time = 0;       // Earliest time this flow can be serviced based on RT/LS and UL curves
                                      // This could be actual time (e.g. nanoseconds from epoch) or virtual time
    uint64_t virtual_finish_time_ul = 0; // Tracks the VFT based on UL curve for the last packet of this flow

    core::FlowId parent_id = 0; // 0 or a special constant for no parent
    std::vector<core::FlowId> children_ids; // List of child flow/class IDs

    // Statistics or other per-flow state can be added here
    // uint64_t current_bytes_in_queue = 0;

    // Constructor
    explicit HfscFlowState(core::FlowId id = 0, core::FlowId p_id = 0) : flow_id(id), parent_id(p_id) {}
};

/**
 * @brief Basic structure for a Hierarchical Fair Service Curve (HFSC) Scheduler.
 *
 * This is a placeholder implementation focusing on the structure.
 * The actual HFSC logic (virtual time calculations, event management) is complex
 * and will be implemented in later phases.
 *
 * Note: For this implementation, PacketDescriptor::priority is used as the
 * core::FlowId to determine which flow's queue to enqueue into.
 */
class HfscScheduler : public SchedulerInterface {
public:
    struct FlowConfig {
        core::FlowId id;
        core::FlowId parent_id; // 0 for root, or ID of parent class
        ServiceCurve real_time_sc;
        ServiceCurve link_share_sc;   // Optional, defaults to 0 if not specified
        ServiceCurve upper_limit_sc;  // Optional, defaults to 0 (no limit) if not specified

        FlowConfig(core::FlowId flow_id, core::FlowId p_id, ServiceCurve rt_sc,
                   ServiceCurve ls_sc = {}, ServiceCurve ul_sc = {})
            : id(flow_id), parent_id(p_id), real_time_sc(rt_sc), link_share_sc(ls_sc), upper_limit_sc(ul_sc) {}
    };

    /**
     * @brief Constructs an HfscScheduler.
     * @param flow_configs Vector of FlowConfig defining per-flow service curves.
     * @param total_link_bandwidth_bps Total bandwidth of the link this scheduler operates on.
     */
    explicit HfscScheduler(const std::vector<FlowConfig>& flow_configs, uint64_t total_link_bandwidth_bps);

    ~HfscScheduler() override = default;

    HfscScheduler(const HfscScheduler&) = delete;
    HfscScheduler& operator=(const HfscScheduler&) = delete;
    HfscScheduler(HfscScheduler&&) = default;
    HfscScheduler& operator=(HfscScheduler&&) = default;

    /**
     * @brief Enqueues a packet. (Placeholder - actual HFSC logic is more complex)
     * PacketDescriptor::priority is used as core::FlowId.
     * @param packet The packet to enqueue.
     * @throws std::logic_error if scheduler is not configured.
     * @throws std::out_of_range if Flow ID from packet is not found.
     */
    void enqueue(PacketDescriptor packet) override;

    /**
     * @brief Dequeues a packet. (Placeholder - actual HFSC logic is far more complex)
     * @return The dequeued PacketDescriptor.
     * @throws std::logic_error if scheduler is not configured or internal error.
     * @throws std::runtime_error if the scheduler is empty.
     */
    PacketDescriptor dequeue() override;

    /**
     * @brief Checks if the scheduler is empty.
     * @return True if no packets are stored, false otherwise.
     */
    bool is_empty() const override;

    // Optional: Additional inspection methods
    size_t get_num_configured_flows() const;
    size_t get_flow_queue_size(core::FlowId flow_id) const;

private:
    std::map<core::FlowId, HfscFlowState> flow_states_; // Stores state for each configured flow
    uint64_t total_link_bandwidth_bps_;
    uint64_t current_virtual_time_ = 0; // System virtual time, crucial for HFSC
    size_t total_packets_ = 0;
    bool is_configured_ = false;

    // Placeholder for actual HFSC internal functions like:
    // void update_flow_eligibility(HfscFlowState& flow_state, const PacketDescriptor& packet); // Old name
    // void update_virtual_times(HfscFlowState& flow_state, const PacketDescriptor& packet); // Old name
    // HfscFlowState* select_next_flow_to_service();
    uint64_t calculate_packet_service_time_us(uint32_t packet_length_bytes, const ServiceCurve& sc) const;
    void update_flow_schedule(core::FlowId flow_id, bool is_newly_active);


    // Nested struct for managing eligible flows in the priority queue
    struct EligibleFlow {
        uint64_t virtual_finish_time; // The time used for ordering in the eligible set
        core::FlowId flow_id;

        // Comparator for min-heap (smallest virtual_finish_time has highest priority)
        // std::priority_queue is a max-heap by default, so std::greater makes it a min-heap.
        bool operator>(const EligibleFlow& other) const {
            if (virtual_finish_time != other.virtual_finish_time) {
                return virtual_finish_time > other.virtual_finish_time;
            }
            // Tie-breaking for stability or consistent ordering if VFTs are equal
            return flow_id > other.flow_id;
        }
    };

    std::priority_queue<EligibleFlow, std::vector<EligibleFlow>, std::greater<EligibleFlow>> eligible_set_;
};

} // namespace scheduler
} // namespace hqts

#endif // HQTS_SCHEDULER_HFSC_SCHEDULER_H_
