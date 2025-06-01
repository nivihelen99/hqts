#include "hqts/scheduler/strict_priority_scheduler.h"
#include <string> // Required for std::to_string in error messages

namespace hqts {
namespace scheduler {

StrictPriorityScheduler::StrictPriorityScheduler(size_t num_priority_levels)
    : num_levels_(num_priority_levels), total_packets_(0) {
    if (num_priority_levels == 0) {
        throw std::invalid_argument("Number of priority levels must be greater than 0.");
    }
    priority_queues_.resize(num_levels_);
}

void StrictPriorityScheduler::enqueue(PacketDescriptor packet) {
    if (packet.priority >= num_levels_) {
        throw std::out_of_range("Packet priority " + std::to_string(packet.priority) +
                                " is out of range. Max allowed is " + std::to_string(num_levels_ - 1) + ".");
    }
    priority_queues_[packet.priority].push(std::move(packet)); // Use std::move if PacketDescriptor is movable
    total_packets_++;
}

PacketDescriptor StrictPriorityScheduler::dequeue() {
    if (is_empty()) {
        throw std::runtime_error("Scheduler is empty, cannot dequeue.");
    }

    // Iterate from the highest priority queue down to the lowest
    // Numerically higher priority value means higher scheduling priority
    for (int i = static_cast<int>(num_levels_ - 1); i >= 0; --i) {
        if (!priority_queues_[static_cast<size_t>(i)].empty()) {
            PacketDescriptor packet = priority_queues_[static_cast<size_t>(i)].front();
            priority_queues_[static_cast<size_t>(i)].pop();
            total_packets_--;
            return packet;
        }
    }
    // This part should ideally not be reached if total_packets_ is managed correctly
    // and is_empty() is checked. If it is reached, it implies an inconsistency.
    throw std::logic_error("Scheduler state inconsistent: is_empty() was false, but no packet found.");
}

bool StrictPriorityScheduler::is_empty() const {
    return total_packets_ == 0;
}

size_t StrictPriorityScheduler::get_num_priority_levels() const {
    return num_levels_;
}

size_t StrictPriorityScheduler::get_queue_size(uint8_t priority_level) const {
    if (priority_level >= num_levels_) {
        throw std::out_of_range("Priority level " + std::to_string(priority_level) +
                                " is out of range. Max allowed is " + std::to_string(num_levels_ - 1) + ".");
    }
    return priority_queues_[priority_level].size();
}

} // namespace scheduler
} // namespace hqts
