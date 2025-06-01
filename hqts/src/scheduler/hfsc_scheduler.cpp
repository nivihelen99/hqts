#include "hqts/scheduler/hfsc_scheduler.h"
#include <limits>  // For std::numeric_limits
#include <string>  // For std::to_string in error messages
#include <algorithm> // For std::max

namespace hqts {
namespace scheduler {

// Private helper method
uint64_t HfscScheduler::calculate_packet_service_time_us(uint32_t packet_length_bytes, const ServiceCurve& sc) const { // Made const
    if (sc.rate_bps == 0) {
        return std::numeric_limits<uint64_t>::max();
    }
    return (static_cast<uint64_t>(packet_length_bytes) * 8 * 1000000) / sc.rate_bps;
}

// New private helper method for updating flow schedule
void HfscScheduler::update_flow_schedule(core::FlowId flow_id, bool is_newly_active) {
    HfscFlowState& flow_state = flow_states_.at(flow_id); // Assume flow_id is valid
    if (flow_state.packet_queue.empty()) {
        return; // Nothing to schedule
    }

    uint32_t packet_len_bytes = flow_state.packet_queue.front().packet_length_bytes;
    uint64_t base_eligible_time;

    if (is_newly_active) {
        // For a newly active flow, eligibility is based on its previous (possibly 0) finish time
        // or the current scheduler virtual time, whichever is later.
        base_eligible_time = std::max(current_virtual_time_, flow_state.virtual_finish_time);
    } else {
        // For a flow that just had a packet dequeued, its next packet's eligibility
        // starts from the current scheduler virtual time (which is the VFT of the dequeued packet).
        base_eligible_time = current_virtual_time_;
    }

    // 1. Calculate RT/LS chosen eligible time and VFT for the flow itself
    uint64_t chosen_eligible_time_self = 0;
    uint64_t chosen_vft_self = std::numeric_limits<uint64_t>::max();
    uint64_t service_time_self = std::numeric_limits<uint64_t>::max();

    bool rt_self_valid = flow_state.real_time_sc.rate_bps > 0;
    bool ls_self_valid = flow_state.link_share_sc.rate_bps > 0;

    uint64_t eligible_time_rt_self = base_eligible_time + flow_state.real_time_sc.delay_us;
    uint64_t vft_rt_self = eligible_time_rt_self + calculate_packet_service_time_us(packet_len_bytes, flow_state.real_time_sc);

    uint64_t eligible_time_ls_self = base_eligible_time + flow_state.link_share_sc.delay_us;
    uint64_t vft_ls_self = eligible_time_ls_self + calculate_packet_service_time_us(packet_len_bytes, flow_state.link_share_sc);

    if (rt_self_valid && ls_self_valid) {
        if (vft_rt_self <= vft_ls_self) {
            chosen_vft_self = vft_rt_self;
            chosen_eligible_time_self = eligible_time_rt_self;
        } else {
            chosen_vft_self = vft_ls_self;
            chosen_eligible_time_self = eligible_time_ls_self;
        }
    } else if (rt_self_valid) {
        chosen_vft_self = vft_rt_self;
        chosen_eligible_time_self = eligible_time_rt_self;
    } else if (ls_self_valid) {
        chosen_vft_self = vft_ls_self;
        chosen_eligible_time_self = eligible_time_ls_self;
    }

    if (chosen_vft_self != std::numeric_limits<uint64_t>::max()) {
        service_time_self = chosen_vft_self - chosen_eligible_time_self;
    }

    // 2. Apply UL constraint for the flow itself
    uint64_t final_eligible_time_self = chosen_eligible_time_self;
    uint64_t final_vft_self = chosen_vft_self;

    if (flow_state.upper_limit_sc.rate_bps > 0) {
        uint64_t eligible_time_ul_self_candidate = std::max(base_eligible_time, flow_state.virtual_finish_time_ul) + flow_state.upper_limit_sc.delay_us;
        final_eligible_time_self = std::max(chosen_eligible_time_self, eligible_time_ul_self_candidate);

        if (chosen_vft_self != std::numeric_limits<uint64_t>::max()) {
            final_vft_self = final_eligible_time_self + service_time_self;
        } else { // No valid RT/LS, so only UL could apply if it could grant eligibility (not standard HFSC)
            // For now, if RT/LS is not valid, this flow does not become eligible from its own curves.
            // This means final_vft_self remains max unless RT/LS was valid.
        }
    }

    // 3. Consider parent constraints (for a two-level hierarchy)
    uint64_t final_eligible_time = final_eligible_time_self;
    uint64_t final_vft = final_vft_self;

    if (flow_state.parent_id != 0) { // Assuming 0 is NO_PARENT_HFSC_FLOW_ID
        auto parent_iter = flow_states_.find(flow_state.parent_id);
        if (parent_iter != flow_states_.end()) {
            HfscFlowState& parent_state = parent_iter->second;

            // Parent's eligibility for this child's packet. Parent doesn't have its own queue.
            // Base eligibility for parent is also tricky: is it based on its own previous VFT for *any* child, or current_VT?
            // Let's assume it's similar to a flow: max(current_VT, parent_state.virtual_finish_time for its own (agg) service)
            uint64_t parent_base_eligible_time = std::max(current_virtual_time_, parent_state.virtual_finish_time);

            uint64_t eligible_time_rt_parent = parent_base_eligible_time + parent_state.real_time_sc.delay_us;
            uint64_t vft_rt_parent = eligible_time_rt_parent + calculate_packet_service_time_us(packet_len_bytes, parent_state.real_time_sc);

            uint64_t eligible_time_ls_parent = parent_base_eligible_time + parent_state.link_share_sc.delay_us;
            uint64_t vft_ls_parent = eligible_time_ls_parent + calculate_packet_service_time_us(packet_len_bytes, parent_state.link_share_sc);

            uint64_t chosen_vft_parent = std::numeric_limits<uint64_t>::max();
            uint64_t chosen_eligible_time_parent = 0;

            bool rt_parent_valid = parent_state.real_time_sc.rate_bps > 0;
            bool ls_parent_valid = parent_state.link_share_sc.rate_bps > 0;

            if (rt_parent_valid && ls_parent_valid) {
                if (vft_rt_parent <= vft_ls_parent) {
                    chosen_vft_parent = vft_rt_parent; chosen_eligible_time_parent = eligible_time_rt_parent;
                } else {
                    chosen_vft_parent = vft_ls_parent; chosen_eligible_time_parent = eligible_time_ls_parent;
                }
            } else if (rt_parent_valid) {
                chosen_vft_parent = vft_rt_parent; chosen_eligible_time_parent = eligible_time_rt_parent;
            } else if (ls_parent_valid) {
                chosen_vft_parent = vft_ls_parent; chosen_eligible_time_parent = eligible_time_ls_parent;
            }

            uint64_t final_eligible_time_parent = chosen_eligible_time_parent;
            if (parent_state.upper_limit_sc.rate_bps > 0) {
                 uint64_t eligible_time_ul_parent_cand = std::max(parent_base_eligible_time, parent_state.virtual_finish_time_ul) + parent_state.upper_limit_sc.delay_us;
                 final_eligible_time_parent = std::max(chosen_eligible_time_parent, eligible_time_ul_parent_cand);
            }

            // The child's final eligibility is constrained by parent's final eligibility for this packet
            final_eligible_time = std::max(final_eligible_time_self, final_eligible_time_parent);
            // The VFT is this new eligible time + the child's own service time (as parent doesn't add to service time, only start time)
            if (service_time_self != std::numeric_limits<uint64_t>::max()) {
                 final_vft = final_eligible_time + service_time_self;
            } else { // Child itself has no valid curve, so it cannot be scheduled.
                 final_vft = std::numeric_limits<uint64_t>::max();
            }
        }
    }

    // 4. Update flow state and add to eligible set
    if (final_vft != std::numeric_limits<uint64_t>::max()) {
        flow_state.virtual_start_time = final_eligible_time;
        flow_state.virtual_finish_time = final_vft;

        if (flow_state.upper_limit_sc.rate_bps > 0) {
             // This update is for the UL VFT of the current packet being scheduled.
             flow_state.virtual_finish_time_ul = final_eligible_time + calculate_packet_service_time_us(packet_len_bytes, flow_state.upper_limit_sc);
        }
        eligible_set_.push({final_vft, flow_id});
    }
}


HfscScheduler::HfscScheduler(const std::vector<FlowConfig>& flow_configs, uint64_t total_link_bandwidth_bps)
    : total_link_bandwidth_bps_(total_link_bandwidth_bps),
      current_virtual_time_(0),
      total_packets_(0),
      is_configured_(false) {

    if (total_link_bandwidth_bps_ == 0) {
        // Allow, but scheduler won't do much.
    }

    if (flow_configs.empty()) {
        is_configured_ = false;
        return;
    }

    // Temporary map to build parent-child relationships
    std::map<core::FlowId, std::vector<core::FlowId>> parent_to_children_map;

    for (const auto& fc : flow_configs) {
        if (flow_states_.count(fc.id)) {
            throw std::invalid_argument("HFSC Scheduler: Duplicate FlowId " + std::to_string(fc.id) + " in configuration.");
        }
        if (fc.id == fc.parent_id && fc.id != 0) { // Check for self-parenting, assuming 0 means no parent
             throw std::invalid_argument("HFSC Scheduler: FlowId " + std::to_string(fc.id) + " cannot be its own parent.");
        }

        HfscFlowState new_flow_state(fc.id, fc.parent_id);
        new_flow_state.real_time_sc = fc.real_time_sc;
        new_flow_state.link_share_sc = fc.link_share_sc;
        new_flow_state.upper_limit_sc = fc.upper_limit_sc;
        new_flow_state.virtual_finish_time = 0;
        new_flow_state.eligible_time = 0;
        new_flow_state.virtual_finish_time_ul = 0;

        flow_states_.emplace(fc.id, new_flow_state);

        if (fc.parent_id != 0) { // Assuming 0 means no parent
            parent_to_children_map[fc.parent_id].push_back(fc.id);
        }
    }

    // Populate children_ids and validate parent existence
    for (const auto& pair : parent_to_children_map) {
        core::FlowId parent_id = pair.first;
        const std::vector<core::FlowId>& children = pair.second;

        auto parent_iter = flow_states_.find(parent_id);
        if (parent_iter == flow_states_.end()) {
            throw std::invalid_argument("HFSC Scheduler: Parent FlowId " + std::to_string(parent_id) + " not found in configuration.");
        }
        parent_iter->second.children_ids = children;
    }

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

    flow_state.packet_queue.push(std::move(packet));
    total_packets_++;

    if (was_empty) {
        update_flow_schedule(target_flow_id, true);
    }
}

PacketDescriptor HfscScheduler::dequeue() {
    if (!is_configured_) {
        throw std::logic_error("HFSC Scheduler: Not configured. Cannot dequeue.");
    }
    if (is_empty()) {
        throw std::runtime_error("HFSC Scheduler: Scheduler is empty (total_packets is 0).");
    }

    if (eligible_set_.empty()) {
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
    // This VFT was determined by RT/LS and potentially constrained by UL.
    current_virtual_time_ = next_to_service.virtual_finish_time;

    // Update the UL finish time state for this flow based on the packet that was JUST dequeued.
    // The packet started service at its flow_state.virtual_start_time (which was the final_eligible_time).
    // This should use the virtual_start_time that was determined when this packet (packet_to_send)
    // was made eligible and added to the eligible_set.
    // next_to_service.virtual_finish_time is the VFT from RT/LS/UL combination.
    // The actual start time for UL calculation should be what its final_eligible_time was.
    // flow_state.virtual_start_time currently holds the start time for the *next* packet if calculated.
    // This is tricky. Let's assume next_to_service stored the final_eligible_time as its virtual_start_time.
    // For now, we use the virtual_start_time that was set for the packet just dequeued.
    // This means flow_state.virtual_start_time before this dequeue call was the start time of packet_to_send.
    // This detail is subtle: virtual_start_time on flow_state is for the *next* packet.
    // The one stored in EligibleFlow is what we need for the VFT that got it scheduled.
    // The problem description implies `P_current.virtual_start_time` is available.
    // Let's assume `next_to_service.virtual_finish_time - service_time_of_packet_to_send_on_governing_curve`
    // was its start time. This is also complex.
    // A simpler way: the UL VFT is updated based on when it *could* have started by UL rules.
    // The `flow_state.virtual_start_time` that was used to put `next_to_service` into `eligible_set_` is key.
    // For now, let's use the `current_virtual_time_` (which is `next_to_service.virtual_finish_time`)
    // and subtract the service time of the packet *on the curve that governed its VFT in eligible_set*.
    // This is still not quite right.
    // Correct logic: When P_current is chosen, its chosen_eligible_time was flow_state.virtual_start_time.
    // So, after dequeuing P_current:
    if (flow_state.upper_limit_sc.rate_bps > 0) {
         flow_state.virtual_finish_time_ul = flow_state.virtual_start_time + // This virtual_start_time was its actual scheduled start
                                           calculate_packet_service_time_us(packet_to_send.packet_length_bytes, flow_state.upper_limit_sc);
    }


    if (!flow_state.packet_queue.empty()) {
        // Flow has more packets, recalculate its eligibility and VFT for the new head packet, then re-add to eligible set.
        uint64_t base_eligible_time_for_next_pkt = current_virtual_time_; // Next packet eligibility starts from current scheduler virtual time
        uint32_t next_packet_len_bytes = flow_state.packet_queue.front().packet_length_bytes;

        // 1. Calculate RT/LS chosen eligible time and VFT
        uint64_t chosen_eligible_time_rt_ls = 0;
        uint64_t chosen_vft_rt_ls = std::numeric_limits<uint64_t>::max();
        uint64_t service_time_rt_ls = std::numeric_limits<uint64_t>::max();

        bool rt_curve_valid = flow_state.real_time_sc.rate_bps > 0;
        bool ls_curve_valid = flow_state.link_share_sc.rate_bps > 0;

        uint64_t eligible_time_rt = base_eligible_time_for_next_pkt + flow_state.real_time_sc.delay_us;
        uint64_t vft_rt = eligible_time_rt + calculate_packet_service_time_us(next_packet_len_bytes, flow_state.real_time_sc);

        uint64_t eligible_time_ls = base_eligible_time_for_next_pkt + flow_state.link_share_sc.delay_us;
        uint64_t vft_ls = eligible_time_ls + calculate_packet_service_time_us(next_packet_len_bytes, flow_state.link_share_sc);

        if (rt_curve_valid && ls_curve_valid) {
            if (vft_rt <= vft_ls) {
                chosen_vft_rt_ls = vft_rt;
                chosen_eligible_time_rt_ls = eligible_time_rt;
            } else {
                chosen_vft_rt_ls = vft_ls;
                chosen_eligible_time_rt_ls = eligible_time_ls;
            }
        } else if (rt_curve_valid) {
            chosen_vft_rt_ls = vft_rt;
            chosen_eligible_time_rt_ls = eligible_time_rt;
        } else if (ls_curve_valid) {
            chosen_vft_rt_ls = vft_ls;
            chosen_eligible_time_rt_ls = eligible_time_ls;
        }

        if (chosen_vft_rt_ls != std::numeric_limits<uint64_t>::max()) {
            service_time_rt_ls = chosen_vft_rt_ls - chosen_eligible_time_rt_ls;
        }

        // 2. Apply UL constraint
        uint64_t final_eligible_time = chosen_eligible_time_rt_ls;
        uint64_t final_vft = chosen_vft_rt_ls;

        if (flow_state.upper_limit_sc.rate_bps > 0) {
            // UL eligibility is based on its own timeline (virtual_finish_time_ul)
            uint64_t eligible_time_ul_candidate = std::max(current_virtual_time_, flow_state.virtual_finish_time_ul) + flow_state.upper_limit_sc.delay_us;
            final_eligible_time = std::max(chosen_eligible_time_rt_ls, eligible_time_ul_candidate);

            if (chosen_vft_rt_ls != std::numeric_limits<uint64_t>::max()) { // If RT/LS provided a valid schedule
                final_vft = final_eligible_time + service_time_rt_ls; // VFT based on RT/LS service time but potentially delayed start
            } else {
                // Only UL is shaping this (e.g. RT/LS rates are 0). This case is less common for re-eval.
                // If RT/LS are not valid, this flow shouldn't be scheduled unless UL alone can schedule it.
                // This part of logic might need refinement if UL can independently make a flow eligible.
                // For now, if no RT/LS path, it means chosen_vft_rt_ls is max, so final_vft remains max.
            }
        }

        // 3. Update flow state and re-add to eligible set
        if (final_vft != std::numeric_limits<uint64_t>::max()) {
            flow_state.virtual_start_time = final_eligible_time; // This is the actual start for the next packet
            flow_state.virtual_finish_time = final_vft;      // This is its VFT for priority queue

            // The virtual_finish_time_ul for *this next packet* will be calculated when it's dequeued,
            // based on its actual start_time (final_eligible_time).
            // So, no update to flow_state.virtual_finish_time_ul here for P_next, only for P_current earlier.
            eligible_set_.push({final_vft, flow_state.flow_id});
        }
    }
    // If flow_state.packet_queue is now empty, it's implicitly not re-added to eligible_set_
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
