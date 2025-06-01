#include "hqts/scheduler/wrr_scheduler.h"
#include <string> // For std::to_string in error messages
#include <numeric> // For std::gcd if a more complex deficit scheme was used, not directly needed now.

namespace hqts {
namespace scheduler {

WrrScheduler::WrrScheduler(const std::vector<QueueConfig>& queue_configs)
    : current_queue_index_(0), total_packets_(0), is_configured_(false) {
    if (queue_configs.empty()) {
        throw std::invalid_argument("WRR Scheduler: queue_configs cannot be empty.");
    }

    queues_.reserve(queue_configs.size());
    for (size_t i = 0; i < queue_configs.size(); ++i) {
        const auto& qc = queue_configs[i];
        if (qc.weight == 0) {
            throw std::invalid_argument("WRR Scheduler: Queue weight for ID " + std::to_string(qc.id) + " cannot be zero.");
        }
        if (queue_id_to_index_.count(qc.id)) {
            throw std::invalid_argument("WRR Scheduler: Duplicate QueueId " + std::to_string(qc.id) + " in configuration.");
        }

        queues_.emplace_back(qc.id, qc.weight);
        // Initialize current_deficit. Common practice is to start with weight, or 0.
        // Let's start with 0, and they get replenished before first service attempt if all are 0.
        // Or, to start them with their weight to allow immediate service:
        queues_.back().current_deficit = static_cast<int32_t>(qc.weight);
        queue_id_to_index_[qc.id] = i;
    }
    is_configured_ = true;
}

void WrrScheduler::enqueue(PacketDescriptor packet) {
    if (!is_configured_) {
        throw std::logic_error("WRR Scheduler: Not configured. Call constructor with queue configurations.");
    }

    // Using packet.priority as the core::QueueId for this scheduler
    core::QueueId target_queue_external_id = static_cast<core::QueueId>(packet.priority);

    auto it = queue_id_to_index_.find(target_queue_external_id);
    if (it == queue_id_to_index_.end()) {
        throw std::out_of_range("WRR Scheduler: QueueId " + std::to_string(target_queue_external_id) +
                                " (from packet.priority) not configured for this scheduler.");
    }

    queues_[it->second].packet_queue.push(std::move(packet));
    total_packets_++;
}

void WrrScheduler::replenish_all_deficits() {
    for (InternalQueueState& q_state : queues_) {
        q_state.current_deficit += static_cast<int32_t>(q_state.weight);
    }
}

PacketDescriptor WrrScheduler::dequeue() {
    if (!is_configured_) {
        throw std::logic_error("WRR Scheduler: Not configured.");
    }
    if (is_empty()) {
        throw std::runtime_error("WRR Scheduler: Scheduler is empty, cannot dequeue.");
    }

    size_t num_queues = queues_.size();
    size_t queues_checked_this_round = 0;
    bool deficits_replenished_this_attempt = false;

    while (true) { // Loop indefinitely until a packet is dequeued or an error state is identified
        queues_checked_this_round = 0;
        bool can_service_any_queue_in_current_state = false;

        for (size_t i = 0; i < num_queues; ++i) {
            // Start check from current_queue_index_ and wrap around
            size_t actual_index = (current_queue_index_ + i) % num_queues;
            InternalQueueState& current_q_state = queues_[actual_index];

            if (!current_q_state.packet_queue.empty() && current_q_state.current_deficit > 0) {
                can_service_any_queue_in_current_state = true; // Found a queue that could be serviced

                PacketDescriptor packet = current_q_state.packet_queue.front();
                current_q_state.packet_queue.pop();
                current_q_state.current_deficit--;
                total_packets_--;

                // Advance current_queue_index_ to the next queue for the next dequeue operation.
                // WRR typically advances after a packet is sent, or after a queue empties its deficit.
                // For simple packet WRR, advance after each packet.
                current_queue_index_ = (actual_index + 1) % num_queues;
                return packet;
            }
        }

        // If we've gone through all queues and couldn't service any (e.g. all non-empty queues have 0 deficit)
        if (!can_service_any_queue_in_current_state) {
            if (deficits_replenished_this_attempt) {
                // This should not happen if there are packets. It means deficits were replenished,
                // but still no queue could be serviced (e.g. all queues became empty simultaneously,
                // but total_packets_ > 0, which is an inconsistency).
                 throw std::logic_error("WRR Scheduler: Inconsistent state. Deficits replenished but no packet dequeued while not empty.");
            }
            replenish_all_deficits();
            deficits_replenished_this_attempt = true; // Mark that we've tried replenishing in this dequeue call
            // Restart the scan from current_queue_index_ (or 0 if preferred for strict round start)
            // current_queue_index_ remains, so the next iteration of the while(true) loop will retry.
        } else {
            // This case should ideally not be hit if the inner loop correctly services a queue.
            // If can_service_any_queue_in_current_state is true, a packet should have been returned.
            // If it's false, the block above should have been hit.
            // This is a safeguard.
        }
         // The while(true) loop continues, either after replenishing or if some logic error prevented dequeue.
    }
}


bool WrrScheduler::is_empty() const {
    if (!is_configured_) {
        // Or return true, or handle as per desired semantics for unconfigured state
        throw std::logic_error("WRR Scheduler: Not configured.");
    }
    return total_packets_ == 0;
}

size_t WrrScheduler::get_queue_size(core::QueueId queue_id) const {
    if (!is_configured_) {
        throw std::logic_error("WRR Scheduler: Not configured.");
    }
    auto it = queue_id_to_index_.find(queue_id);
    if (it == queue_id_to_index_.end()) {
        throw std::out_of_range("WRR Scheduler: QueueId " + std::to_string(queue_id) + " not configured.");
    }
    return queues_[it->second].packet_queue.size();
}

size_t WrrScheduler::get_num_queues() const {
    if (!is_configured_) {
        // Or return 0, or handle as per desired semantics for unconfigured state
        throw std::logic_error("WRR Scheduler: Not configured.");
    }
    return queues_.size();
}

} // namespace scheduler
} // namespace hqts
