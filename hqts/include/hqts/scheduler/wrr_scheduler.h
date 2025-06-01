#ifndef HQTS_SCHEDULER_WRR_SCHEDULER_H_
#define HQTS_SCHEDULER_WRR_SCHEDULER_H_

#include "hqts/scheduler/scheduler_interface.h"
// #include "hqts/scheduler/queue_types.h" // No longer directly used for PacketQueue
#include "hqts/scheduler/aqm_queue.h"     // For RedAqmQueue and RedAqmParameters
#include "hqts/core/flow_context.h"     // For core::QueueId

#include <vector>
#include <map>
#include <stdexcept> // For std::out_of_range, std::invalid_argument, std::runtime_error, std::logic_error
#include <string>    // Potentially for error messages
#include <numeric>   // For std::accumulate or other numeric operations if needed later
#include <memory>    // For std::unique_ptr if that was chosen for queue storage

namespace hqts {
namespace scheduler {

/**
 * @brief Implements a packet-based Weighted Round Robin (WRR) Scheduler.
 *
 * This scheduler serves a configured set of queues. Each queue has a weight.
 * In each round, queues are visited. A queue can dequeue a number of packets
 * proportional to its weight (in this simple packet WRR, it means it gets
 * 'weight' chances to send one packet if its deficit allows).
 *
 * Note: For this implementation, PacketDescriptor::priority is used as the
 * core::QueueId to determine which queue to enqueue into. This is a simplification.
 */
class WrrScheduler : public SchedulerInterface {
public:
    struct QueueConfig {
        core::QueueId id; // User-defined ID for the queue
        uint32_t weight;  // Weight for this queue (must be > 0)
        RedAqmParameters aqm_params; // New member

        QueueConfig(core::QueueId q_id, uint32_t w, RedAqmParameters aqm_p)
            : id(q_id), weight(w), aqm_params(std::move(aqm_p)) {}
    };

    /**
     * @brief Constructs a WrrScheduler.
     * @param queue_configs A vector of QueueConfig structs defining the queues and their weights.
     * @throws std::invalid_argument if queue_configs is empty or any queue has weight 0.
     */
    explicit WrrScheduler(const std::vector<QueueConfig>& queue_configs);

    ~WrrScheduler() override = default;

    // Delete copy and move operations for simplicity.
    WrrScheduler(const WrrScheduler&) = delete;
    WrrScheduler& operator=(const WrrScheduler&) = delete;
    WrrScheduler(WrrScheduler&&) = default;
    WrrScheduler& operator=(WrrScheduler&&) = default;

    /**
     * @brief Enqueues a packet. PacketDescriptor::priority is used as core::QueueId.
     * @param packet The packet to enqueue.
     * @throws std::logic_error if scheduler is not configured.
     * @throws std::out_of_range if packet.priority (as QueueId) is not a configured queue.
     */
    void enqueue(PacketDescriptor packet) override;

    /**
     * @brief Dequeues a packet according to WRR discipline.
     * @return The dequeued PacketDescriptor.
     * @throws std::runtime_error if the scheduler is empty.
     * @throws std::logic_error if scheduler is not configured or in an inconsistent state.
     */
    PacketDescriptor dequeue() override;

    /**
     * @brief Checks if the scheduler is empty (no packets in any queue).
     * @return True if all queues are empty, false otherwise.
     * @throws std::logic_error if scheduler is not configured.
     */
    bool is_empty() const override;

    // Optional: Additional inspection methods
    /**
     * @brief Gets the current number of packets in a specific queue.
     * @param queue_id The external ID of the queue.
     * @return The number of packets in that queue.
     * @throws std::out_of_range if queue_id is not a configured queue.
     */
    size_t get_queue_size(core::QueueId queue_id) const;

    /**
     * @brief Gets the number of configured queues.
     * @return The number of queues.
     */
    size_t get_num_queues() const;


private:
    struct InternalQueueState {
        RedAqmQueue packet_queue; // Changed type
        uint32_t weight;
        int32_t current_deficit;
        core::QueueId external_id; // User-facing ID

        InternalQueueState(core::QueueId ext_id, uint32_t w, const RedAqmParameters& aqm_p)
            : packet_queue(aqm_p), weight(w), current_deficit(static_cast<int32_t>(w)), external_id(ext_id) {}
    };

    std::vector<InternalQueueState> queues_;
    std::map<core::QueueId, size_t> queue_id_to_index_; // Maps external QueueId to index in queues_ vector

    size_t current_queue_index_ = 0; // Index for the next queue to consider in the round-robin scheme
    size_t total_packets_ = 0;       // Total packets across all queues
    bool is_configured_ = false;     // Tracks if constructor successfully configured queues

    // Helper to replenish deficits for all queues if a full round yields no serviceable queue.
    void replenish_all_deficits();
};

} // namespace scheduler
} // namespace hqts

#endif // HQTS_SCHEDULER_WRR_SCHEDULER_H_
