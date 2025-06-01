#ifndef HQTS_SCHEDULER_AQM_QUEUE_H_
#define HQTS_SCHEDULER_AQM_QUEUE_H_

#include "hqts/scheduler/packet_descriptor.h"
#include "hqts/scheduler/queue_types.h" // For PacketQueue (std::queue<PacketDescriptor>)
#include <cstdint>
#include <deque> // std::deque is often more suitable for average queue size tracking than std::queue
#include <string> // For potential error messages
#include <stdexcept> // For exceptions
#include <chrono> // For time if needed for EWMA weight calculation or stats

namespace hqts {
namespace scheduler {

// Parameters for RED AQM
struct RedAqmParameters {
    uint32_t min_threshold_bytes;  // Minimum average queue size before any drops
    uint32_t max_threshold_bytes;  // Average queue size at which drop probability reaches max_p
    double max_probability;        // Maximum drop probability (e.g., 0.1 for 10%)
    double ewma_weight;            // Weight for EWMA average queue size calculation (e.g., 0.002)
    uint32_t queue_capacity_bytes; // Physical capacity of the queue

    // Constructor for sensible defaults or specific settings
    RedAqmParameters(uint32_t min_t, uint32_t max_t, double max_p, double weight, uint32_t capacity)
        : min_threshold_bytes(min_t), max_threshold_bytes(max_t),
          max_probability(max_p), ewma_weight(weight), queue_capacity_bytes(capacity) {
        if (min_t == 0 || max_t == 0 || capacity == 0 || min_t >= max_t || max_t > capacity || weight <= 0.0 || weight >= 1.0 || max_p <= 0.0 || max_p > 1.0) {
            // Consider more specific error messages for each condition if desired
            throw std::invalid_argument("Invalid RED AQM parameters provided to constructor.");
        }
    }
};

// Interface for an AQM-enabled queue (optional, can directly implement RedAqmQueue)
// For now, let's proceed directly with RedAqmQueue. If other AQMs are added, an interface can be refactored.

class RedAqmQueue {
public:
    explicit RedAqmQueue(const RedAqmParameters& params);

    // Attempts to enqueue a packet. Returns true on success, false if dropped by RED or if queue is at physical capacity.
    // The packet is consumed (moved or copied) only if successfully enqueued.
    bool enqueue(PacketDescriptor packet);

    // Dequeues a packet. Throws std::runtime_error if empty.
    PacketDescriptor dequeue();

    // Returns true if the queue is empty.
    bool is_empty() const;

    // Returns the current number of packets in the queue.
    size_t get_current_packet_count() const;

    // Returns the current total byte size of packets in the queue.
    uint32_t get_current_byte_size() const;

    // Returns the current calculated average queue size in bytes.
    double get_average_queue_size_bytes() const;

    // Gets the configured RED parameters.
    const RedAqmParameters& get_parameters() const;

private:
    // Calculates the current drop probability based on average queue size.
    double calculate_drop_probability() const;

    // Updates the EWMA average queue size.
    // Takes current physical queue size in bytes.
    // 'queue_was_idle' helps in one EWMA variant: avg = (1-w) * avg + w * current OR avg = current if idle.
    // For RED, typically avg = (1-w)*avg + w*current_physical_size always when queue is sampled (e.g. on packet arrival).
    void update_average_queue_size();


    std::deque<PacketDescriptor> packet_buffer_; // Using std::deque for efficient front/back access
    RedAqmParameters params_;
    double average_queue_size_bytes_ = 0.0;
    uint32_t current_total_bytes_ = 0; // Current actual total bytes in queue

    // For RED's count since last drop (used to smooth drop probability over time)
    // This makes RED less bursty in its drops.
    int packets_since_last_drop_ = 0;

    // Random number generation for probabilistic drop (to be added in .cpp)
    // Needs to be mutable if used in const calculate_drop_probability, or make that non-const.
    // For now, keep calculate_drop_probability const, implying random generation is handled carefully.
    // A common approach is to have a non-const "should_drop_packet" method that uses it.
    // Or, pass a random generator reference.
    // For simplicity in header, we omit its declaration here, will be in .cpp.
    // mutable std::mt19937 random_generator_;
    // mutable std::uniform_real_distribution<double> probability_distribution_;
};

} // namespace scheduler
} // namespace hqts

#endif // HQTS_SCHEDULER_AQM_QUEUE_H_
