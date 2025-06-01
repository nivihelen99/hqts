#include "hqts/scheduler/strict_priority_scheduler.h"
#include <string> // Required for std::to_string in error messages

#include "hqts/scheduler/aqm_queue.h" // Required for RedAqmParameters

namespace hqts {
namespace scheduler {

StrictPriorityScheduler::StrictPriorityScheduler(const std::vector<RedAqmParameters>& queue_params_list)
    : num_levels_(queue_params_list.size()), total_packets_(0) {
    if (num_levels_ == 0) {
        throw std::invalid_argument("StrictPriorityScheduler: queue_params_list cannot be empty.");
    }
    priority_queues_.reserve(num_levels_);
    for (const auto& params : queue_params_list) {
        priority_queues_.emplace_back(params);
    }
}

void StrictPriorityScheduler::enqueue(PacketDescriptor packet) {
    if (packet.priority >= num_levels_) {
        throw std::out_of_range("StrictPriorityScheduler: Packet priority " + std::to_string(packet.priority) +
                                " is out of range. Max allowed is " + std::to_string(num_levels_ - 1) + ".");
    }
    // RedAqmQueue::enqueue returns true if packet is accepted, false if dropped by AQM or full.
    if (priority_queues_[packet.priority].enqueue(std::move(packet))) {
        total_packets_++;
    }
    // If enqueue returns false, the packet was dropped by the AQM logic within RedAqmQueue,
    // so we don't increment total_packets_.
}

PacketDescriptor StrictPriorityScheduler::dequeue() {
    if (is_empty()) {
        throw std::runtime_error("StrictPriorityScheduler: Scheduler is empty, cannot dequeue.");
    }

    // Iterate from the highest priority queue down to the lowest
    // Numerically higher priority value means higher scheduling priority
    for (size_t i = 0; i < num_levels_; ++i) {
        // Highest priority is num_levels_ - 1, lowest is 0.
        // Vector index matches priority level directly (e.g. priority_queues_[0] is prio 0)
        // So, loop from (num_levels_ - 1) down to 0.
        size_t current_priority_index = num_levels_ - 1 - i;
        if (!priority_queues_[current_priority_index].is_empty()) {
            PacketDescriptor packet = priority_queues_[current_priority_index].dequeue();
            total_packets_--;
            return packet;
        }
    }
    // This part should ideally not be reached if total_packets_ is managed correctly
    // and is_empty() is checked. If it is reached, it implies an inconsistency.
    throw std::logic_error("StrictPriorityScheduler: State inconsistent. is_empty() was false, but no packet found.");
}

bool StrictPriorityScheduler::is_empty() const {
    return total_packets_ == 0;
}

size_t StrictPriorityScheduler::get_num_priority_levels() const {
    return num_levels_;
}

size_t StrictPriorityScheduler::get_queue_size(uint8_t priority_level) const {
    if (priority_level >= num_levels_) {
        throw std::out_of_range("StrictPriorityScheduler: Priority level " + std::to_string(priority_level) +
                                " is out of range. Max allowed is " + std::to_string(num_levels_ - 1) + ".");
    }
    return priority_queues_[priority_level].get_current_packet_count();
}

} // namespace scheduler
} // namespace hqts
