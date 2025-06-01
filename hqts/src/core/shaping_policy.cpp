#include "hqts/core/shaping_policy.h"

#include <chrono> // For std::chrono::steady_clock

namespace hqts {
namespace core {

ShapingPolicy::ShapingPolicy(
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
    bool p_drop_on_red,
    uint8_t prio_g,
    uint8_t prio_y,
    uint8_t prio_r,
    core::QueueId qid_g,
    core::QueueId qid_y,
    core::QueueId qid_r
) : id(p_id),
    parent_id(p_parent_id),
    // children_ids is intentionally left empty on construction, to be populated later
    name(std::move(p_name)),
    committed_rate_bps(p_committed_rate_bps),
    peak_rate_bps(p_peak_rate_bps),
    committed_burst_bytes(p_committed_burst_bytes),
    excess_burst_bytes(p_excess_burst_bytes),
    algorithm(p_algorithm),
    weight(p_weight),
    priority_level(p_priority_level), // Original priority for the class itself
    drop_on_red(p_drop_on_red),
    target_priority_green(prio_g),
    target_priority_yellow(prio_y),
    target_priority_red(prio_r),
    target_queue_id_green(qid_g),
    target_queue_id_yellow(qid_y),
    target_queue_id_red(qid_r),
    cir_bucket(p_committed_rate_bps, p_committed_burst_bytes),
    pir_bucket(p_peak_rate_bps, p_excess_burst_bytes),
    // stats members are default initialized to 0 (as per struct definition in .h)
    last_updated(std::chrono::steady_clock::now())
{
    // children_ids is initialized by default (empty vector) via its default constructor
    // stats is initialized by default (all members to 0)
}

} // namespace core
} // namespace hqts
