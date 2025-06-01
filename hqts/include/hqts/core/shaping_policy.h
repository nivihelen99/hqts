#ifndef HQTS_CORE_SHAPING_POLICY_H_
#define HQTS_CORE_SHAPING_POLICY_H_

#include "hqts/policy/policy_types.h"
#include "hqts/core/token_bucket.h"
#include "hqts/core/flow_context.h" // For core::QueueId

#include <cstdint>
#include <vector>
#include <string>
#include <chrono>

namespace hqts {
namespace core {

struct PolicyStatistics {
    uint64_t bytes_processed = 0;
    uint64_t packets_processed = 0;
    uint64_t bytes_dropped = 0;
    uint64_t packets_dropped = 0;
    // Potentially add more detailed stats like:
    // uint64_t conforming_bytes = 0;
    // uint64_t excess_bytes = 0;
    // std::chrono::steady_clock::time_point last_packet_time;
};

struct ShapingPolicy {
    policy::PolicyId id;
    policy::PolicyId parent_id; // Use policy::NO_PARENT_POLICY_ID for root policies
    std::vector<policy::PolicyId> children_ids;
    std::string name; // Optional: for easier identification

    // Rate limiting parameters
    uint64_t committed_rate_bps;  // CIR in bits per second
    uint64_t peak_rate_bps;       // PIR in bits per second (optional, 0 if not used)
    uint64_t committed_burst_bytes; // CBS in bytes
    uint64_t excess_burst_bytes;    // EBS in bytes (related to PIR)

    // Scheduling parameters
    policy::SchedulingAlgorithm algorithm;
    uint32_t weight; // Weight for WFQ, WRR, DRR
    policy::Priority priority_level; // Priority for STRICT_PRIORITY or as a tie-breaker

    // Policy-to-Queue mapping and DSCP marking parameters
    bool drop_on_red;                // If true, RED packets are dropped, else marked/re-prioritized
    uint8_t target_priority_green;   // Target scheduler priority for GREEN packets
    uint8_t target_priority_yellow;  // Target scheduler priority for YELLOW packets
    uint8_t target_priority_red;     // Target scheduler priority for RED packets (if not dropped)
    core::QueueId target_queue_id_green;  // Target scheduler QueueId for GREEN packets
    core::QueueId target_queue_id_yellow; // Target scheduler QueueId for YELLOW packets
    core::QueueId target_queue_id_red;    // Target scheduler QueueId for RED packets (if not dropped)
    // DSCP values could also be added here if needed: uint8_t dscp_green, dscp_yellow, dscp_red;


    // Token bucket state
    TokenBucket cir_bucket;
    TokenBucket pir_bucket; // Used if peak_rate_bps > 0 and > committed_rate_bps

    // Statistics
    PolicyStatistics stats;
    std::chrono::steady_clock::time_point last_updated; // Timestamp of the last update to this policy's state or stats

    // Constructor
    ShapingPolicy(
        policy::PolicyId p_id,
        policy::PolicyId p_parent_id,
        std::string p_name,
        uint64_t p_committed_rate_bps,
        uint64_t p_peak_rate_bps,
        uint64_t p_committed_burst_bytes,
        uint64_t p_excess_burst_bytes,
        policy::SchedulingAlgorithm p_algorithm,
        uint32_t p_weight,
        policy::Priority p_priority_level,
        // New params for mapping:
        bool p_drop_on_red = true,
        uint8_t prio_g = 7,
        uint8_t prio_y = 4,
        uint8_t prio_r = 1,
        core::QueueId qid_g = 0,
        core::QueueId qid_y = 0,
        core::QueueId qid_r = 0
    );

    // Default constructor might be needed by Boost.MultiIndex if not all members are initialized by the main constructor
    ShapingPolicy() = default; // Add default constructor if needed by multi_index_container
};

} // namespace core
} // namespace hqts

#endif // HQTS_CORE_SHAPING_POLICY_H_
