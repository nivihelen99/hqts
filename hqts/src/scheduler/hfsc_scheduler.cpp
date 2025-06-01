#include "hqts/scheduler/hfsc_scheduler.h"
#include <limits>  // For std::numeric_limits
#include <string>  // For std::to_string in error messages
#include <algorithm> // For std::max

namespace hqts {
namespace scheduler {

// Private helper method
uint64_t HfscScheduler::calculate_packet_service_time_us(uint32_t packet_length_bytes, const ServiceCurve& sc) {
    if (sc.rate_bps == 0) {
        return std::numeric_limits<uint64_t>::max(); // Effectively infinite time for zero rate
    }
    // service_time_us = (packet_length_bytes * 8 bits/byte * 1,000,000 us/sec) / rate_bits_per_sec
    return (static_cast<uint64_t>(packet_length_bytes) * 8 * 1000000) / sc.rate_bps;
}

HfscScheduler::HfscScheduler(const std::vector<FlowConfig>& flow_configs, uint64_t total_link_bandwidth_bps)
    : total_link_bandwidth_bps_(total_link_bandwidth_bps),
      current_virtual_time_(0),
      total_packets_(0),
      is_configured_(false) {

    if (total_link_bandwidth_bps_ == 0) {
        // This could be an error or a specific state meaning no service.
        // For now, allow it, but scheduler won't do much.
    }

    if (flow_configs.empty()) {
        is_configured_ = false;
        return;
    }

    for (const auto& fc : flow_configs) {
        if (flow_states_.count(fc.id)) {
            throw std::invalid_argument("HFSC Scheduler: Duplicate FlowId " + std::to_string(fc.id) + " in configuration.");
        }

        HfscFlowState new_flow_state(fc.id);
        new_flow_state.real_time_sc = fc.real_time_sc;
        new_flow_state.link_share_sc = fc.link_share_sc;
        new_flow_state.upper_limit_sc = fc.upper_limit_sc;

        // Initialize virtual_finish_time to 0, indicating no prior service.
        // eligible_time also starts at 0 or based on current_virtual_time_ + delay from RT SC.
        // For a newly configured flow that is initially idle, its VFT can be considered 0.
        // Its eligibility will be determined when the first packet arrives.
        new_flow_state.virtual_finish_time = 0; // Or current_virtual_time_
        new_flow_state.eligible_time = 0;       // Or current_virtual_time_ + new_flow_state.real_time_sc.delay_us

        flow_states_.emplace(fc.id, new_flow_state);
    }

    // eligible_set_ is default initialized (empty priority queue)
    is_configured_ = !flow_configs.empty();
}

void HfscScheduler::enqueue(PacketDescriptor packet) {
    if (!is_configured_) {
        throw std::logic_error("HFSC Scheduler: Not configured or no flows defined. Cannot enqueue.");
    }

    core::FlowId target_flow_id = static_cast<core::FlowId>(packet.priority);
    auto flow_state_iter = flow_states_.find(target_flow_id);

    if (flow_state_iter == flow_states_.end()) {
        throw std::out_of_range("HFSC Scheduler: Flow ID " + std::to_string(target_flow_id) +
                                " (from packet.priority) not found in HFSC configuration.");
    }

    HfscFlowState& flow_state = flow_state_iter->second;
    bool was_empty = flow_state.packet_queue.empty();

    flow_state.packet_queue.push(std::move(packet)); // Use std::move for efficiency
    total_packets_++;

    if (was_empty) { // Flow was idle, now becomes active
        // Update eligibility and virtual times based on the new packet and RT curve.
        // This is a simplified model focusing on the RT curve for initial eligibility.
        // eligible_time is the later of current system virtual time or the flow's previous VFT (if any).
        // Then, add the RT curve's specific delay.
        flow_state.eligible_time = std::max(current_virtual_time_, flow_state.virtual_finish_time);
        flow_state.eligible_time += flow_state.real_time_sc.delay_us;

        uint64_t service_time_us = calculate_packet_service_time_us(
            flow_state.packet_queue.front().packet_length_bytes,
            flow_state.real_time_sc
        );

        if (service_time_us == std::numeric_limits<uint64_t>::max() && flow_state.real_time_sc.rate_bps == 0) {
            // If RT SC rate is zero, this flow might not be eligible for RT service
            // or its VFT would be infinite. Handle as per specific HFSC variant.
            // For now, if RT rate is 0, it won't be added to eligible_set based on RT VFT.
            // This flow would then rely on LS SC if defined.
            // This part needs more sophisticated handling for combined SCs.
            // For this simplified step, we assume RT SC has a non-zero rate if used for eligibility.
            // If rate is 0, it effectively won't be added to eligible_set via this path.
        } else {
            flow_state.virtual_start_time = flow_state.eligible_time;
            flow_state.virtual_finish_time = flow_state.virtual_start_time + service_time_us;
            eligible_set_.push({flow_state.virtual_finish_time, flow_state.flow_id});
        }
    }
    // If not was_empty, the flow is already in eligible_set_ (or should be).
    // Its VFT will be updated when its current head packet is dequeued.
}

PacketDescriptor HfscScheduler::dequeue() {
    if (!is_configured_) {
        throw std::logic_error("HFSC Scheduler: Not configured. Cannot dequeue.");
    }
    if (is_empty()) { // Checks total_packets_
        throw std::runtime_error("HFSC Scheduler: Scheduler is empty (total_packets is 0).");
    }

    if (eligible_set_.empty()) {
        // This state implies total_packets_ > 0 but no flow is currently eligible.
        // This can happen if all active flows have future eligible_times.
        // Full HFSC: Advance current_virtual_time_ to min eligible_time of non-empty flows,
        // then re-evaluate eligibility for that flow and add to eligible_set_.
        // Simplified for this step:
        throw std::logic_error("HFSC Scheduler: Eligible set is empty, but packets exist. "
                               "Needs advanced virtual time update logic (TODO).");
    }

    EligibleFlow next_to_service = eligible_set_.top();
    eligible_set_.pop();

    core::FlowId selected_flow_id = next_to_service.flow_id;
    HfscFlowState& flow_state = flow_states_.at(selected_flow_id); // .at() throws if not found (consistency check)

    if (flow_state.packet_queue.empty()) {
        // This indicates an inconsistency: flow was in eligible_set_ but its queue is empty.
        // This might happen if not properly removed when it became empty previously.
        throw std::logic_error("HFSC Scheduler: Selected eligible flow has empty packet queue. Flow ID: " + std::to_string(selected_flow_id));
    }

    PacketDescriptor packet_to_send = flow_state.packet_queue.front();
    flow_state.packet_queue.pop();
    total_packets_--;

    // Advance scheduler's virtual time to the virtual finish time of the serviced packet.
    current_virtual_time_ = next_to_service.virtual_finish_time;

    if (!flow_state.packet_queue.empty()) {
        // Flow has more packets, recalculate its VFT for the new head packet and re-add to eligible set.
        const auto& next_packet_in_queue = flow_state.packet_queue.front();

        // Eligibility for the next packet starts from the finish of the current one (current_virtual_time_).
        // Add RT SC delay.
        flow_state.eligible_time = current_virtual_time_ + flow_state.real_time_sc.delay_us;

        uint64_t service_time_us = calculate_packet_service_time_us(
            next_packet_in_queue.packet_length_bytes,
            flow_state.real_time_sc
        );

        if (service_time_us == std::numeric_limits<uint64_t>::max() && flow_state.real_time_sc.rate_bps == 0) {
            // Similar to enqueue: if RT rate is 0, it won't be re-added based on RT VFT.
            // Needs more sophisticated handling for LS SC.
        } else {
            flow_state.virtual_start_time = flow_state.eligible_time;
            flow_state.virtual_finish_time = flow_state.virtual_start_time + service_time_us;
            eligible_set_.push({flow_state.virtual_finish_time, flow_state.flow_id});
        }
    }
    // If flow_state.packet_queue is now empty, it's implicitly removed from consideration for eligible_set_
    // until a new packet arrives via enqueue().

    return packet_to_send;
}

bool HfscScheduler::is_empty() const {
    // If not configured, it's effectively empty of manageable packets.
    if (!is_configured_) return true;
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
