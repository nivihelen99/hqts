#ifndef HQTS_POLICY_POLICY_TYPES_H_
#define HQTS_POLICY_POLICY_TYPES_H_

#include <cstdint>

namespace hqts {
namespace policy {

using PolicyId = uint64_t;

enum class SchedulingAlgorithm {
    WFQ,              // Weighted Fair Queuing
    WRR,              // Weighted Round Robin
    STRICT_PRIORITY,  // Strict Priority
    DRR,              // Deficit Round Robin
    HFSC              // Hierarchical Fair Service Curve
};

using Priority = uint8_t; // Represents scheduling priority level, e.g., 0-7

// Special Policy ID to indicate no parent (for root policies)
constexpr PolicyId NO_PARENT_POLICY_ID = 0; // Or std::numeric_limits<PolicyId>::max();

} // namespace policy
} // namespace hqts

#endif // HQTS_POLICY_POLICY_TYPES_H_
