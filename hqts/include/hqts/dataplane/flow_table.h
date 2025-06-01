#ifndef HQTS_DATAPLANE_FLOW_TABLE_H_
#define HQTS_DATAPLANE_FLOW_TABLE_H_

#include "hqts/core/flow_context.h" // For core::FlowId and core::FlowContext
#include <unordered_map>

namespace hqts {
namespace core { // Definition should be in namespace hqts::core

// FlowTable maps a unique FlowId to its context.
// Note: core::FlowId and core::FlowContext are defined in "hqts/core/flow_context.h"
using FlowTable = std::unordered_map<FlowId, FlowContext>;

} // namespace core
} // namespace hqts

#endif // HQTS_DATAPLANE_FLOW_TABLE_H_
