#include "hqts/scheduler/drr_scheduler.h"
#include "hqts/scheduler/aqm_queue.h" // For RedAqmParameters
#include <string> // For std::to_string in error messages
#include <stdexcept> // For exceptions

namespace hqts {
namespace scheduler {

DrrScheduler::DrrScheduler(const std::vector<QueueConfig>& queue_configs)
    : current_queue_index_(0), total_packets_(0), is_configured_(false) {
    if (queue_configs.empty()) {
        throw std::invalid_argument("DRR Scheduler: queue_configs cannot be empty.");
    }

    queues_.reserve(queue_configs.size());
    for (size_t i = 0; i < queue_configs.size(); ++i) {
        const auto& qc = queue_configs[i];
        if (qc.quantum_bytes == 0) {
            throw std::invalid_argument("DRR Scheduler: Queue quantum for ID " + std::to_string(qc.id) + " must be greater than zero.");
        }
        if (queue_id_to_index_.count(qc.id)) {
            throw std::invalid_argument("DRR Scheduler: Duplicate QueueId " + std::to_string(qc.id) + " in configuration.");
        }

        // Use the updated InternalQueueState constructor that takes RedAqmParameters
        queues_.emplace_back(qc.id, qc.quantum_bytes, qc.aqm_params);
        // Deficit counter for each queue starts at 0 (handled by InternalQueueState constructor).
        queue_id_to_index_[qc.id] = i;
    }
    is_configured_ = true;
}

void DrrScheduler::enqueue(PacketDescriptor packet) {
    if (!is_configured_) {
        throw std::logic_error("DRR Scheduler: Not configured. Call constructor with queue configurations.");
    }

    core::QueueId target_queue_external_id = static_cast<core::QueueId>(packet.priority);

    auto it = queue_id_to_index_.find(target_queue_external_id);
    if (it == queue_id_to_index_.end()) {
        throw std::out_of_range("DRR Scheduler: QueueId " + std::to_string(target_queue_external_id) +
                                " (from packet.priority) not configured for this scheduler.");
    }

    // Enqueue into RedAqmQueue; increment total_packets_ only if successful
    if (queues_[it->second].packet_queue.enqueue(std::move(packet))) {
        total_packets_++;
    }
    // If RedAqmQueue::enqueue returns false, packet was dropped by AQM, total_packets_ not incremented.
}

PacketDescriptor DrrScheduler::dequeue() {
    if (!is_configured_) {
        throw std::logic_error("DRR Scheduler: Not configured.");
    }
    if (is_empty()) {
        throw std::runtime_error("DRR Scheduler: Scheduler is empty, cannot dequeue.");
    }

    size_t num_queues = queues_.size();
    size_t search_attempts = 0; // To prevent potential infinite loops in unforeseen edge cases

    while (search_attempts < num_queues * 2) { // Allow cycling through queues multiple times if needed
        InternalQueueState& current_q_state = queues_[current_queue_index_];

        if (!current_q_state.packet_queue.is_empty()) {
            // Add quantum for this service opportunity if it's the start of its "turn"
            // or if its deficit was previously too low for the front packet.
            // A common DRR adds quantum when the queue is selected and non-empty.
            current_q_state.deficit_counter += static_cast<int64_t>(current_q_state.quantum_bytes);

            // Service as many packets as current deficit allows from this queue
            // (Standard DRR services one packet if deficit allows, then moves to next queue in active list.
            //  More aggressive DRR might send multiple. This implementation sends one then moves.)
            if (!current_q_state.packet_queue.is_empty() && // Re-check, could be empty if last packet just sent
                current_q_state.deficit_counter >= current_q_state.packet_queue.front().packet_length_bytes) {

                uint32_t packet_len = current_q_state.packet_queue.front().packet_length_bytes;
                PacketDescriptor packet_to_send = current_q_state.packet_queue.dequeue(); // Dequeue also removes

                current_q_state.deficit_counter -= packet_len;
                total_packets_--;

                current_queue_index_ = (current_queue_index_ + 1) % num_queues;
                return packet_to_send;
            }
        }

        // Move to the next queue
        current_queue_index_ = (current_queue_index_ + 1) % num_queues;
        search_attempts++;
    }

    // If this point is reached, it means after several full scans, no packet could be dequeued
    // even though is_empty() was false. This implies all remaining packets are too large for
    // their queues' deficit (even after adding quantum) or some other logic error.
    // The deficit carries over, so subsequent calls should eventually allow sending.
    // However, to break a potential live-lock in a test or unforeseen scenario:
    if (total_packets_ > 0) { // Check again, as state might have changed for re-entrant calls (not expected here)
         throw std::logic_error("DRR Scheduler: Exceeded search cycles. No suitable packet found despite scheduler not being empty. Possible issue with packet sizes vs quanta or internal state.");
    }
    // If total_packets_ somehow became 0 during the loop (e.g. by another thread not an issue here)
    // then throw the standard empty error.
    throw std::runtime_error("DRR Scheduler: Scheduler became empty during dequeue search (unexpected).");
}


bool DrrScheduler::is_empty() const {
    if (!is_configured_) {
        // Consider if an unconfigured scheduler should be treated as empty or throw.
        // Returning true if not configured prevents operations on an invalid state.
        return true;
    }
    return total_packets_ == 0;
}

size_t DrrScheduler::get_queue_size(core::QueueId queue_id) const {
    if (!is_configured_) {
        throw std::logic_error("DRR Scheduler: Not configured.");
    }
    auto it = queue_id_to_index_.find(queue_id);
    if (it == queue_id_to_index_.end()) {
        throw std::out_of_range("DRR Scheduler: QueueId " + std::to_string(queue_id) + " not configured.");
    }
    return queues_[it->second].packet_queue.get_current_packet_count();
}

size_t DrrScheduler::get_num_queues() const {
    if (!is_configured_) {
        return 0; // Or throw std::logic_error("DRR Scheduler: Not configured.");
    }
    return queues_.size();
}

} // namespace scheduler
} // namespace hqts
