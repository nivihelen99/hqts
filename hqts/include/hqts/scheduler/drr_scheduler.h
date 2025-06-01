#ifndef HQTS_SCHEDULER_DRR_SCHEDULER_H_
#define HQTS_SCHEDULER_DRR_SCHEDULER_H_

#include "hqts/scheduler/scheduler_interface.h"
// #include "hqts/scheduler/queue_types.h" // No longer directly used for PacketQueue
#include "hqts/scheduler/aqm_queue.h"     // For RedAqmQueue and RedAqmParameters
#include "hqts/core/flow_context.h"     // For core::QueueId

#include <vector>
#include <map>
#include <stdexcept> // For std::out_of_range, std::invalid_argument, std::runtime_error, std::logic_error
#include <memory>    // For std::unique_ptr if that was chosen
#include <string>    // Potentially for error messages

namespace hqts {
namespace scheduler {

/**
 * @brief Implements a Deficit Round Robin (DRR) Scheduler.
 *
 * This scheduler serves a configured set of queues. Each queue has a quantum (in bytes).
 * Queues are visited in a round-robin fashion. When a queue is visited, its deficit
 * counter is incremented by its quantum. It can then send packets as long as its
 * deficit is sufficient for the packet size. The deficit is reduced by the size of
 * each sent packet.
 *
 * Note: For this implementation, PacketDescriptor::priority is used as the
 * core::QueueId to determine which queue to enqueue into. This is a simplification.
 */
class DrrScheduler : public SchedulerInterface {
public:
    struct QueueConfig {
        core::QueueId id;        // User-defined ID for the queue
        uint32_t quantum_bytes; // Quantum in bytes for this queue (must be > 0)
        RedAqmParameters aqm_params; // AQM parameters for this queue

        // Updated constructor
        QueueConfig(core::QueueId q_id, uint32_t q_bytes, RedAqmParameters aqm_p)
            : id(q_id), quantum_bytes(q_bytes), aqm_params(std::move(aqm_p)) {}
    };

    /**
     * @brief Constructs a DrrScheduler.
     * @param queue_configs A vector of QueueConfig structs.
     * @throws std::invalid_argument if queue_configs is empty or any queue has quantum_bytes = 0 or duplicate IDs.
     */
    explicit DrrScheduler(const std::vector<QueueConfig>& queue_configs);

    ~DrrScheduler() override = default;

    DrrScheduler(const DrrScheduler&) = delete;
    DrrScheduler& operator=(const DrrScheduler&) = delete;
    DrrScheduler(DrrScheduler&&) = default;
    DrrScheduler& operator=(DrrScheduler&&) = default;

    /**
     * @brief Enqueues a packet. PacketDescriptor::priority is used as core::QueueId.
     * @param packet The packet to enqueue.
     * @throws std::logic_error if scheduler is not configured.
     * @throws std::out_of_range if packet.priority (as QueueId) is not a configured queue.
     */
    void enqueue(PacketDescriptor packet) override;

    /**
     * @brief Dequeues a packet according to DRR discipline.
     * @return The dequeued PacketDescriptor.
     * @throws std::runtime_error if the scheduler is empty.
     * @throws std::logic_error if scheduler is not configured or in an inconsistent state.
     */
    PacketDescriptor dequeue() override;

    /**
     * @brief Checks if the scheduler is empty.
     * @return True if all queues are empty, false otherwise.
     * @throws std::logic_error if scheduler is not configured.
     */
    bool is_empty() const override;

    /**
     * @brief Gets the current number of packets in a specific queue.
     * @param queue_id The external ID of the queue.
     * @return The number of packets in that queue.
     * @throws std::out_of_range if queue_id is not a configured queue.
     * @throws std::logic_error if scheduler is not configured.
     */
    size_t get_queue_size(core::QueueId queue_id) const;

    /**
     * @brief Gets the number of configured queues.
     * @return The number of queues.
     * @throws std::logic_error if scheduler is not configured.
     */
    size_t get_num_queues() const;

private:
    struct InternalQueueState {
        RedAqmQueue packet_queue; // Changed type
        uint32_t quantum_bytes;
        int64_t deficit_counter;
        core::QueueId external_id;

        // Updated constructor
        InternalQueueState(core::QueueId ext_id, uint32_t q_bytes, const RedAqmParameters& aqm_p)
            : packet_queue(aqm_p), quantum_bytes(q_bytes), deficit_counter(0), external_id(ext_id) {}
    };

    std::vector<InternalQueueState> queues_;
    std::map<core::QueueId, size_t> queue_id_to_index_;

    size_t current_queue_index_ = 0; // Index for the next queue to consider in the round-robin scheme
    size_t total_packets_ = 0;
    bool is_configured_ = false;

    // Optional: To keep track of queues that are non-empty and thus part of the "active round".
    // This can optimize dequeue by not iterating over empty queues repeatedly.
    // std::vector<size_t> active_queue_indices_;
    // void update_active_list_on_enqueue(size_t internal_idx);
    // void update_active_list_on_dequeue(size_t internal_idx);
};

} // namespace scheduler
} // namespace hqts

#endif // HQTS_SCHEDULER_DRR_SCHEDULER_H_
