#ifndef HQTS_SCHEDULER_STRICT_PRIORITY_SCHEDULER_H_
#define HQTS_SCHEDULER_STRICT_PRIORITY_SCHEDULER_H_

#include "hqts/scheduler/scheduler_interface.h"
// #include "hqts/scheduler/queue_types.h" // PacketQueue no longer directly used
#include "hqts/scheduler/aqm_queue.h"     // For RedAqmQueue and RedAqmParameters

#include <vector>
#include <stdexcept> // For std::out_of_range, std::invalid_argument, std::runtime_error
#include <string>    // Potentially for error messages, though not strictly required by declarations
#include <memory>    // For std::unique_ptr, if that route was chosen

namespace hqts {
namespace scheduler {

/**
 * @brief Implements a Strict Priority Scheduler.
 *
 * This scheduler manages multiple priority queues. Packets are always dequeued
 * from the highest-priority non-empty queue. Numerically higher priority values
 * are treated as higher scheduling priority (e.g., priority 7 is served before priority 0).
 */
class StrictPriorityScheduler : public SchedulerInterface {
public:
    /**
     * @brief Constructs a StrictPriorityScheduler with AQM-enabled queues.
     * @param queue_params_list A vector of RedAqmParameters, one for each priority queue.
     *                          The number of elements determines the number of priority levels.
     * @throws std::invalid_argument if queue_params_list is empty.
     */
    explicit StrictPriorityScheduler(const std::vector<RedAqmParameters>& queue_params_list);

    ~StrictPriorityScheduler() override = default;

    // Delete copy and move operations for simplicity, as managing underlying queues could be complex.
    // If needed, these can be implemented with proper deep copy/move semantics.
    StrictPriorityScheduler(const StrictPriorityScheduler&) = delete;
    StrictPriorityScheduler& operator=(const StrictPriorityScheduler&) = delete;
    StrictPriorityScheduler(StrictPriorityScheduler&&) = default; // Default move is fine if members are movable
    StrictPriorityScheduler& operator=(StrictPriorityScheduler&&) = default; // Default move is fine

    /**
     * @brief Enqueues a packet based on its priority.
     * @param packet The packet to enqueue. packet.priority determines the queue.
     * @throws std::out_of_range if packet.priority is >= num_priority_levels.
     */
    void enqueue(PacketDescriptor packet) override;

    /**
     * @brief Dequeues a packet from the highest-priority non-empty queue.
     * @return The dequeued PacketDescriptor.
     * @throws std::runtime_error if the scheduler is empty.
     */
    PacketDescriptor dequeue() override;

    /**
     * @brief Checks if the scheduler is empty (no packets in any queue).
     * @return True if all priority queues are empty, false otherwise.
     */
    bool is_empty() const override;

    /**
     * @brief Gets the configured number of priority levels.
     * @return The number of priority levels.
     */
    size_t get_num_priority_levels() const;

    /**
     * @brief Gets the current number of packets in a specific priority queue.
     * @param priority_level The priority level of the queue.
     * @return The number of packets in that queue.
     * @throws std::out_of_range if priority_level is >= num_priority_levels.
     */
    size_t get_queue_size(uint8_t priority_level) const;

private:
    std::vector<RedAqmQueue> priority_queues_; // Now a vector of RedAqmQueues
    const size_t num_levels_;
    size_t total_packets_ = 0; // Total number of packets successfully enqueued across all AQM queues
};

} // namespace scheduler
} // namespace hqts

#endif // HQTS_SCHEDULER_STRICT_PRIORITY_SCHEDULER_H_
