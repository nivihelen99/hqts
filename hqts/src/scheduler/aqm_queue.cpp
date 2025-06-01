#include "hqts/scheduler/aqm_queue.h"
#include <random>      // For std::mt19937, std::uniform_real_distribution
#include <algorithm>   // For std::max, std::min
#include <cmath>       // For std::pow, not strictly needed for linear prob
#include <chrono>      // For seeding random generator
#include <string>      // For std::to_string in error messages (though not used in this version)

namespace hqts {
namespace scheduler {

// Constructor
RedAqmQueue::RedAqmQueue(const RedAqmParameters& params)
    : params_(params),
      average_queue_size_bytes_(0.0),
      current_total_bytes_(0),
      packets_since_last_drop_(0),
      // Seed the random number generator. Using system clock for non-deterministic behavior.
      // For deterministic testing, a fixed seed could be used.
      random_generator_(static_cast<unsigned int>(std::chrono::system_clock::now().time_since_epoch().count())),
      probability_distribution_(0.0, 1.0) {}

// Private helper: Updates EWMA average queue size
// This is called *before* a drop decision for an arriving packet, using current_total_bytes_ (state before arrival).
// Or *after* a packet is dequeued, using current_total_bytes_ (state after departure).
void RedAqmQueue::update_average_queue_size() {
    // Standard EWMA: avg = (1-w)*avg + w*sample
    // Sample is the current instantaneous queue size (current_total_bytes_)
    average_queue_size_bytes_ = (1.0 - params_.ewma_weight) * average_queue_size_bytes_ +
                                params_.ewma_weight * static_cast<double>(current_total_bytes_);
}

// Private helper: Calculates base drop probability (p_b)
double RedAqmQueue::calculate_drop_probability() const {
    if (average_queue_size_bytes_ < params_.min_threshold_bytes) {
        return 0.0;
    } else if (average_queue_size_bytes_ >= params_.max_threshold_bytes) {
        // When avg is at or beyond max_th, base probability is max_p
        return params_.max_probability;
    } else {
        // Linear interpolation between min_th and max_th
        double factor = (average_queue_size_bytes_ - params_.min_threshold_bytes) /
                        (params_.max_threshold_bytes - params_.min_threshold_bytes);
        return factor * params_.max_probability;
    }
}

// Public method: Enqueue
bool RedAqmQueue::enqueue(PacketDescriptor packet) {
    // 1. Update average queue size based on state *before* this packet arrives.
    update_average_queue_size();

    // 2. Check physical capacity first (absolute tail drop if full)
    if (current_total_bytes_ + packet.packet_length_bytes > params_.queue_capacity_bytes) {
        // Physical queue overflow, packet is dropped.
        // RED counters (like packets_since_last_drop_) are typically not reset for physical drops,
        // as these are not probabilistic RED drops.
        return false;
    }

    // 3. Calculate base drop probability (p_b)
    double p_b = calculate_drop_probability();
    double final_drop_prob = 0.0;

    if (p_b > 0.0) { // Only apply gentle RED if base probability is non-zero
        // Apply "gentle" RED modification using packets_since_last_drop_
        // dp = p_b / (1 - count * p_b)
        // This increases drop probability as count increases.
        double denominator = 1.0 - static_cast<double>(packets_since_last_drop_) * p_b;
        if (denominator <= 1e-9) { // Avoid division by zero or very small numbers; effectively means prob is 1
            final_drop_prob = 1.0;
        } else {
            final_drop_prob = p_b / denominator;
        }
        // Ensure probability does not exceed 1.0 due to formula artifacts, though max_p should cap p_b.
        // The formula for p_b already caps it at params_.max_probability.
        // However, the gentle RED adjustment can push it higher.
        // Some RED variants cap the final_drop_prob at 1.0 or a slightly higher max_p (e.g. 2*max_p)
        final_drop_prob = std::min(final_drop_prob, 1.0);
    }
    // If p_b is 0.0, final_drop_prob remains 0.0.


    // 4. Probabilistic drop decision
    if (final_drop_prob > 0.0 && probability_distribution_(random_generator_) < final_drop_prob) {
        // Packet dropped by RED
        packets_since_last_drop_ = 0;
        return false;
    }

    // 5. If not dropped, enqueue the packet
    packets_since_last_drop_++;
    current_total_bytes_ += packet.packet_length_bytes; // Update before pushing for consistency if packet had 0 length
    packet_buffer_.push_back(std::move(packet));
    // Note: If EWMA was to be updated *after* enqueue reflecting the new size, it would be called here again.
    // However, RED typically uses avg queue size *seen by arriving packet*.
    return true;
}

// Public method: Dequeue
PacketDescriptor RedAqmQueue::dequeue() {
    if (is_empty()) {
        throw std::runtime_error("RedAqmQueue: Queue is empty, cannot dequeue.");
    }

    PacketDescriptor packet = packet_buffer_.front();
    packet_buffer_.pop_front();
    current_total_bytes_ -= packet.packet_length_bytes;

    // Update average queue size after a packet departs.
    // This keeps the average fresh for subsequent enqueue operations.
    // Some RED variants might only update EWMA on enqueue.
    // However, updating on dequeue as well gives a more responsive average.
    update_average_queue_size();

    return packet;
}

// Public method: is_empty
bool RedAqmQueue::is_empty() const {
    return packet_buffer_.empty();
}

// Public method: get_current_packet_count
size_t RedAqmQueue::get_current_packet_count() const {
    return packet_buffer_.size();
}

// Public method: get_current_byte_size
uint32_t RedAqmQueue::get_current_byte_size() const {
    return current_total_bytes_;
}

// Public method: get_average_queue_size_bytes
double RedAqmQueue::get_average_queue_size_bytes() const {
    return average_queue_size_bytes_;
}

// Public method: get_parameters
const RedAqmParameters& RedAqmQueue::get_parameters() const {
    return params_;
}

// Public method: front (const)
const PacketDescriptor& RedAqmQueue::front() const {
    if (is_empty()) {
        throw std::runtime_error("RedAqmQueue: front() called on empty queue.");
    }
    return packet_buffer_.front();
}

} // namespace scheduler
} // namespace hqts
