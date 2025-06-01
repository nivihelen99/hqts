#ifndef HQTS_SCHEDULER_SCHEDULER_INTERFACE_H_
#define HQTS_SCHEDULER_SCHEDULER_INTERFACE_H_

#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor
#include "hqts/core/flow_context.h"          // For core::QueueId (though not used in current commented-out methods)

#include <cstdint> // For uint32_t in commented-out methods

namespace hqts {
namespace scheduler {

class SchedulerInterface {
public:
    virtual ~SchedulerInterface() = default;

    /**
     * @brief Enqueues a packet into the scheduler.
     *
     * The scheduler implementation will determine how and where this packet is stored
     * based on its internal logic (e.g., flow ID, priority).
     *
     * @param packet The packet descriptor to enqueue.
     */
    virtual void enqueue(PacketDescriptor packet) = 0;

    /**
     * @brief Dequeues a packet from the scheduler according to its scheduling discipline.
     *
     * The returned packet is the one selected by the scheduler to be processed next.
     * If the scheduler is empty and no packet can be dequeued, the behavior might
     * depend on the implementation (e.g., throw an exception, or return a PacketDescriptor
     * with a special state, though the latter is less common for dequeue).
     * For simplicity, assume it blocks or throws if empty, or users check is_empty() first.
     *
     * @return PacketDescriptor The packet descriptor dequeued.
     */
    virtual PacketDescriptor dequeue() = 0;

    /**
     * @brief Checks if the scheduler is currently empty.
     *
     * An empty scheduler has no packets to dequeue.
     *
     * @return true if the scheduler has no packets, false otherwise.
     */
    virtual bool is_empty() const = 0;

    /**
     * @brief Gets the number of packets currently held by the scheduler.
     * @return The total number of packets across all internal queues.
     */
    // virtual size_t size() const = 0; // Optional: total size

    // --- Dynamic Queue Management Examples (Optional - depends on scheduler type) ---
    // These methods might be relevant for schedulers that allow dynamic configuration
    // of queues (e.g., for different traffic classes or policies).

    /**
     * @brief Adds a new queue to the scheduler (if supported).
     *
     * @param queue_id The unique identifier for the queue to add.
     * @param weight_or_priority The scheduling weight (for WFQ/WRR) or priority level.
     *                           The interpretation depends on the scheduler implementation.
     */
    // virtual void add_queue(core::QueueId queue_id, uint32_t weight_or_priority) = 0;

    /**
     * @brief Removes an existing queue from the scheduler (if supported).
     *
     * @param queue_id The unique identifier for the queue to remove.
     */
    // virtual void remove_queue(core::QueueId queue_id) = 0;

    /**
     * @brief Updates parameters for an existing queue (if supported).
     *
     * @param queue_id The unique identifier for the queue to update.
     * @param new_weight_or_priority The new scheduling weight or priority level.
     */
    // virtual void update_queue(core::QueueId queue_id, uint32_t new_weight_or_priority) = 0;
};

} // namespace scheduler
} // namespace hqts

#endif // HQTS_SCHEDULER_SCHEDULER_INTERFACE_H_
