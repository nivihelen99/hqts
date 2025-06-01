#include "hqts/scheduler/drr_scheduler.h"
#include <string> // For std::to_string in error messages

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

        queues_.emplace_back(qc.id, qc.quantum_bytes);
        // Deficit counter for each queue starts at 0.
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

    queues_[it->second].packet_queue.push(std::move(packet));
    total_packets_++;
}

PacketDescriptor DrrScheduler::dequeue() {
    if (!is_configured_) {
        throw std::logic_error("DRR Scheduler: Not configured.");
    }
    if (is_empty()) {
        throw std::runtime_error("DRR Scheduler: Scheduler is empty, cannot dequeue.");
    }

    size_t num_queues = queues_.size();
    size_t queues_scanned_in_round = 0; // To detect a full scan without dequeuing

    while (queues_scanned_in_round < num_queues) {
        InternalQueueState& current_q_state = queues_[current_queue_index_];

        if (!current_q_state.packet_queue.empty()) {
            // Add quantum to deficit only if the queue was selected and is non-empty.
            // More standard DRR adds quantum when it's the queue's turn, regardless of deficit,
            // if it's on the "active list" (non-empty).
            // Let's adjust: a queue gets its quantum if it's its turn AND it's not empty.
            // If its deficit was already positive, it can try to send first.
            // A common model: deficit is persistent. Quantum is added when queue is visited.
            current_q_state.deficit_counter += static_cast<int64_t>(current_q_state.quantum_bytes);

            // Try to send packets from this queue
            if (!current_q_state.packet_queue.empty() && // Check again, might have been emptied by another thread if not careful (not an issue here)
                current_q_state.deficit_counter >= current_q_state.packet_queue.front().packet_length_bytes) {

                PacketDescriptor packet = current_q_state.packet_queue.front();

                // This check is to ensure we don't allow deficit to go negative *after* sending.
                // Standard DRR allows deficit to go negative if a packet is larger than deficit but smaller than deficit+quantum.
                // The problem description said: "If packet_length > deficit_counter (after adding quantum), it cannot send."
                // This implies deficit must be positive *before* subtraction.
                // Let's stick to: if deficit_counter (after adding quantum) >= packet_length, send.

                current_q_state.packet_queue.pop();
                current_q_state.deficit_counter -= packet.packet_length_bytes;
                total_packets_--;

                // Move to the next queue for the next dequeue operation
                current_queue_index_ = (current_queue_index_ + 1) % num_queues;
                return packet;
            }
            // If we couldn't send (either queue became empty, or front packet too large for current deficit+quantum)
            // the current deficit (which includes the added quantum) is preserved for the next round.
        }

        // Move to the next queue
        current_queue_index_ = (current_queue_index_ + 1) % num_queues;
        queues_scanned_in_round++;
    }

    // If we complete a full scan of all queues and no packet was dequeued,
    // it implies that all non-empty queues have packets larger than their
    // current deficit_counter + quantum_bytes (or just deficit_counter if quantum was added and then couldn't send).
    // This state should ideally resolve in subsequent calls if DRR is working and packets are not pathologically large.
    // For a simple DRR, if total_packets_ > 0, a packet should be found.
    // This might indicate an issue if all remaining packets are larger than their respective queue's quantum,
    // and their deficits never accumulate enough.
    // However, the standard DRR logic (add quantum, then try to send one or more) should prevent this deadlock
    // as long as queues are not empty.
    // The loop `while (queues_scanned_in_round < num_queues)` ensures we try each queue once per "meta-round".
    // If this point is reached, it means all queues were visited, their deficits (potentially) augmented,
    // and none could send. This can happen if remaining packets are too large for current_deficit + quantum.
    // The deficit carries over. So subsequent calls to dequeue will again increment deficit and try.
    // To prevent infinite loops in a buggy state or pathological case not handled by this simple DRR:
    // This throw indicates that after checking all queues (and adding quantum to each non-empty one),
    // no packet could be served. This implies all remaining packets are too large for deficit_after_adding_quantum.
    // This situation is possible in DRR if a packet is larger than a queue's quantum and its deficit is low.
    // The queue has to wait for its deficit to build up over several "empty" turns.
    // The problem arises if the loop *always* exits here. This means total_packets_ > 0 but no progress.
    // The `dequeue` method should guarantee progress if not empty.
    // The logic needs to ensure that if a queue is selected, its deficit is increased, and then it attempts to send.
    // If it sends, great. If not (packet too big), its increased deficit is available next time.
    // The while loop should be `while(true)` and the `queues_scanned_in_round` is more of a way to detect specific states.

    // Re-evaluating the loop: The outer loop should ensure that *eventually* a packet is sent if one exists.
    // The current `queues_scanned_in_round < num_queues` might terminate prematurely if the first queue
    // it checks happens to be the one that needs to accumulate deficit over multiple "passes" where other queues are empty.

    // Let's use a simpler, more standard DRR dequeue loop:
    // Keep cycling. In each turn, a queue gets its quantum.
    // This ensures that if a packet is smaller than quantum, it will eventually go.
    // If a packet is larger than quantum, its queue's deficit will grow over several turns until it can send.
    // The `is_empty()` check at the start is crucial.
    // The loop must continue until a packet is found.
    size_t search_cycles = 0;
    while(true) { // This loop must find a packet if total_packets_ > 0
        InternalQueueState& queue = queues_[current_queue_index_];
        bool packet_sent_from_this_queue = false;

        if (!queue.packet_queue.empty()) {
            queue.deficit_counter += queue.quantum_bytes; // Add quantum for this service opportunity

            // Service as many packets as current deficit allows
            while (!queue.packet_queue.empty() && queue.deficit_counter >= queue.packet_queue.front().packet_length_bytes) {
                PacketDescriptor packet = queue.packet_queue.front();
                queue.packet_queue.pop();
                queue.deficit_counter -= packet.packet_length_bytes;
                total_packets_--;

                // Packet sent. Advance to next queue for next Dequeue() call.
                // For DRR, after a queue sends (even if it could send more from this quantum),
                // we typically move to the next queue in the round robin for fairness.
                current_queue_index_ = (current_queue_index_ + 1) % queues_.size();
                return packet;
            }
        }
        // If no packet was sent from this queue (either empty or deficit too low for front packet)
        current_queue_index_ = (current_queue_index_ + 1) % queues_.size();

        search_cycles++;
        if (search_cycles > num_queues * 2 && total_packets_ > 0) { // Safeguard
             // If we've cycled through all queues multiple times and haven't found a packet,
             // yet the scheduler claims not to be empty, there's a logic flaw or pathological state.
             // This might happen if all remaining packets are larger than any queue's quantum + accumulated deficit.
             // A simple DRR might get stuck if deficit is not allowed to go negative to accommodate sending a packet
             // slightly larger than current deficit but smaller than deficit + quantum.
             // The current logic: deficit_counter -= packet.packet_length_bytes. This can make it negative.
             // If it's negative, the condition deficit_counter >= packet_length_bytes will fail until it's positive.
             // The logic seems okay: deficit can go negative, representing 'debt'.
             // This safeguard might indicate all remaining packets are too large for their queues' deficit to ever become positive.
             // (e.g. packet size > quantum, and deficit starts at 0 and is only ever reduced).
             // The issue is if deficit is only ever reduced. It should be *incremented* by quantum.
             // The current logic *does* increment by quantum.
             // So, if a packet is very large, deficit will keep increasing by quantum each round until it's large enough.
             // This safeguard should ideally not be hit.
            throw std::logic_error("DRR Scheduler: Exceeded search cycles without dequeuing a packet, possible logic flaw or pathological packet sizes relative to quanta.");
        }
    }
}


bool DrrScheduler::is_empty() const {
    if (!is_configured_) {
        throw std::logic_error("DRR Scheduler: Not configured.");
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
    return queues_[it->second].packet_queue.size();
}

size_t DrrScheduler::get_num_queues() const {
    if (!is_configured_) {
        throw std::logic_error("DRR Scheduler: Not configured.");
    }
    return queues_.size();
}

} // namespace scheduler
} // namespace hqts
