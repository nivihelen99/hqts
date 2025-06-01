#include "hqts/core/flow_context.h"

#include <chrono> // For std::chrono::steady_clock

namespace hqts {
namespace core {

FlowContext::FlowContext(FlowId f_id, policy::PolicyId p_id, QueueId q_id, DropPolicy d_policy)
    : flow_id(f_id),
      policy_id(p_id),
      // current_rate_bps is initialized to 0 by default
      // accumulated_bytes_in_period is initialized to 0 by default
      queue_id(q_id),
      // current_queue_depth_bytes is initialized to 0 by default
      drop_policy(d_policy),
      // stats is default constructed (FlowStatistics constructor handles its members)
      // sla_status is initialized to SLAStatus::UNKNOWN by default
      last_packet_processing_time(std::chrono::steady_clock::now()) // Or time_point::min() if no packet yet
{
    // Explicitly initialize time points in stats for clarity upon context creation,
    // though FlowStatistics constructor already handles them.
    // If a flow context is created *before* any packet, these might be better as min() or a flag.
    // However, if created upon first packet, now() is appropriate.
    // For this constructor, assuming it might be created proactively:
    stats.first_packet_time = std::chrono::steady_clock::time_point::min();
    stats.last_packet_time = std::chrono::steady_clock::time_point::min();
}

} // namespace core
} // namespace hqts
