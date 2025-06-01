#include "hqts/scheduler/hfsc_scheduler.h"
#include <limits>  // For std::numeric_limits if needed for time calculations
#include <string>  // For std::to_string in error messages

namespace hqts {
namespace scheduler {

HfscScheduler::HfscScheduler(const std::vector<FlowConfig>& flow_configs, uint64_t total_link_bandwidth_bps)
    : total_link_bandwidth_bps_(total_link_bandwidth_bps),
      current_virtual_time_(0), // Initialize system virtual time
      total_packets_(0),
      is_configured_(false) {

    if (total_link_bandwidth_bps_ == 0) {
        // Depending on HFSC variant, zero bandwidth might be invalid or mean something specific.
        // For now, let's assume it's allowed but the scheduler might not do much.
        // Consider throwing std::invalid_argument if it's strictly disallowed.
    }

    if (flow_configs.empty()) {
        // Scheduler is configured but with no flows. is_configured_ could be true or false.
        // Let's say it's configured if constructor is called, even if no flows.
        // However, most operations would be trivial.
        // The prompt said: "If flow_configs is empty, set is_configured_ = false; and return or throw"
        // Let's follow that:
        is_configured_ = false;
        // Optionally:
        // throw std::invalid_argument("HFSC Scheduler: flow_configs cannot be empty for a functional scheduler.");
        // For this phase, allowing construction with empty config but marking not "fully" configured.
        // The prompt implies not throwing but setting is_configured_ based on emptiness.
        return; // Early exit if no flows, is_configured_ remains false or set based on logic
    }

    for (const auto& fc : flow_configs) {
        if (flow_states_.count(fc.id)) {
            throw std::invalid_argument("HFSC Scheduler: Duplicate FlowId " + std::to_string(fc.id) + " in configuration.");
        }

        HfscFlowState new_flow_state(fc.id);
        new_flow_state.real_time_sc = fc.real_time_sc;
        new_flow_state.link_share_sc = fc.link_share_sc;
        new_flow_state.upper_limit_sc = fc.upper_limit_sc;

        // Initial virtual time calculations would happen here based on service curves
        // For example, eligible_time might be current_virtual_time_ + fc.real_time_sc.delay_us
        // (if virtual time and real delay can be mixed, or convert delay to virtual units).
        // For now, these are default initialized to 0 in HfscFlowState.
        // new_flow_state.eligible_time = current_virtual_time_ + (fc.real_time_sc.delay_us * VIRTUAL_TIME_CONVERSION_FACTOR_IF_ANY);

        flow_states_.emplace(fc.id, new_flow_state);
    }

    is_configured_ = !flow_configs.empty(); // Set true if there were actual configs
}

void HfscScheduler::enqueue(PacketDescriptor packet) {
    if (!is_configured_) {
        throw std::logic_error("HFSC Scheduler: Not configured or no flows defined. Cannot enqueue.");
    }

    core::FlowId target_flow_id = static_cast<core::FlowId>(packet.priority);
    auto it = flow_states_.find(target_flow_id);

    if (it == flow_states_.end()) {
        throw std::out_of_range("HFSC Scheduler: Flow ID " + std::to_string(target_flow_id) +
                                " (from packet.priority) not found in HFSC configuration.");
    }

    // Placeholder enqueue logic:
    it->second.packet_queue.push(std::move(packet));
    total_packets_++;

    // TODO (Phase 3+):
    // 1. Update flow's eligibility time (eligible_time) based on its real-time curve
    //    and the current packet. This involves calculating when the packet *would* have
    //    finished if served by its RT SC, starting from the eligible time of the previous
    //    packet in that flow or current virtual time.
    //    update_flow_eligibility(it->second, packet);
    //
    // 2. If the flow becomes active (was empty), update its virtual start/finish times
    //    based on link-sharing curve and current system virtual time.
    //    update_virtual_times(it->second, packet); // This is complex
    //
    // 3. Add the flow to an "eligible" set/heap, ordered by eligible_time (for RT service)
    //    or virtual_finish_time (for LS service).
}

PacketDescriptor HfscScheduler::dequeue() {
    if (!is_configured_) {
        throw std::logic_error("HFSC Scheduler: Not configured. Cannot dequeue.");
    }
    if (is_empty()) {
        throw std::runtime_error("HFSC Scheduler: Scheduler is empty.");
    }

    // Placeholder dequeue logic:
    // Iterate through all flows, find the first one with a non-empty queue, and serve it.
    // This is NOT HFSC behavior but a basic FIFO across flows for placeholder.
    for (auto& pair : flow_states_) {
        HfscFlowState& current_flow_state = pair.second;
        if (!current_flow_state.packet_queue.empty()) {
            PacketDescriptor p = current_flow_state.packet_queue.front();
            current_flow_state.packet_queue.pop();
            total_packets_--;

            // TODO (Phase 3+):
            // 1. Update system virtual time based on the packet just dequeued and link speed.
            //    current_virtual_time_ += p.packet_length_bytes * VIRTUAL_TIME_UNITS_PER_BYTE;
            //
            // 2. If the flow from which packet was dequeued is still active (has more packets):
            //    - Recalculate its eligible_time / virtual_finish_time for its new head packet.
            //    - Re-insert/update its position in the eligible set/heap.
            //
            // 3. If the flow becomes empty, remove from eligible set/heap.

            return p;
        }
    }

    // This should not be reached if is_empty() is false and total_packets_ is managed correctly.
    throw std::logic_error("HFSC Scheduler: Dequeue called but no packets found - internal state error.");
}

bool HfscScheduler::is_empty() const {
    // is_configured_ check might be desired here too, or let callers handle.
    // If not configured, is it "empty" or in an invalid state to ask?
    // For now, if it holds no packets, it's empty.
    return total_packets_ == 0;
}

size_t HfscScheduler::get_num_configured_flows() const {
    return flow_states_.size();
}

size_t HfscScheduler::get_flow_queue_size(core::FlowId flow_id) const {
    auto it = flow_states_.find(flow_id);
    if (it == flow_states_.end()) {
        throw std::out_of_range("HFSC Scheduler: Flow ID " + std::to_string(flow_id) + " not configured.");
    }
    return it->second.packet_queue.size();
}

} // namespace scheduler
} // namespace hqts
