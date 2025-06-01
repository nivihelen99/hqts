#ifndef HQTS_CORE_FLOW_CONTEXT_H_
#define HQTS_CORE_FLOW_CONTEXT_H_

#include "hqts/policy/policy_types.h" // For PolicyId

#include <cstdint>
#include <chrono>
#include <string> // Currently not used by FlowId, but kept for potential future use

namespace hqts {
namespace core {

// For now, FlowId is a simple type.
// Later, this could be a struct/tuple representing a 5-tuple (srcIP, dstIP, srcPort, dstPort, proto)
// and would require a custom hash for std::unordered_map.
using FlowId = uint64_t;

using QueueId = uint32_t;

enum class DropPolicy {
    TAIL_DROP, // Simple tail drop when queue is full
    RED,       // Random Early Detection
    WRED       // Weighted Random Early Detection
};

struct FlowStatistics {
    uint64_t bytes_processed = 0;
    uint64_t packets_processed = 0;
    uint64_t bytes_dropped = 0;   // Dropped due to shaping, policing, or queue overflow
    uint64_t packets_dropped = 0;
    std::chrono::steady_clock::time_point first_packet_time;
    std::chrono::steady_clock::time_point last_packet_time;

    FlowStatistics() : first_packet_time(std::chrono::steady_clock::time_point::min()), // Or some other indicator of not-yet-seen
                       last_packet_time(std::chrono::steady_clock::time_point::min()) {}
};

enum class SLAStatus {
    CONFORMING,
    NON_CONFORMING,
    UNKNOWN
};

struct FlowContext {
    FlowId flow_id;
    policy::PolicyId policy_id; // ID of the ShapingPolicy applied to this flow

    // Current shaping/policing state indicators (illustrative)
    uint64_t current_rate_bps = 0;          // Current observed rate of the flow (e.g., calculated periodically)
    uint64_t accumulated_bytes_in_period = 0; // Bytes seen since last rate calculation period start

    // Queue management
    QueueId queue_id;                           // Queue this flow is mapped to
    uint32_t current_queue_depth_bytes = 0;    // Current depth of the assigned queue
    DropPolicy drop_policy = DropPolicy::TAIL_DROP;

    // Statistics & SLA
    FlowStatistics stats;
    SLAStatus sla_status = SLAStatus::UNKNOWN;

    std::chrono::steady_clock::time_point last_packet_processing_time; // Timestamp of the last packet processed for this flow

    // Constructors
    FlowContext() = default; // Allow default construction

    FlowContext(FlowId f_id, policy::PolicyId p_id, QueueId q_id, DropPolicy d_policy);
};

} // namespace core
} // namespace hqts

#endif // HQTS_CORE_FLOW_CONTEXT_H_
