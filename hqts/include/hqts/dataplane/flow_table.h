#ifndef HQTS_DATAPLANE_FLOW_TABLE_H_
#define HQTS_DATAPLANE_FLOW_TABLE_H_

#include "hqts/core/flow_context.h" // Provides core::FlowId and core::FlowContext

#include <unordered_map>

namespace hqts {
namespace dataplane {

// If core::FlowId remains a simple type like uint64_t, std::hash<core::FlowId> will work by default.
// If core::FlowId becomes a custom struct (e.g., for a 5-tuple), a specialization
// for std::hash<core::FlowId> will be required for FlowTable to compile.
//
// Example (if FlowId were a struct):
/*
namespace hqts {
namespace core {
struct FlowId5Tuple {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint8_t  protocol;

    bool operator==(const FlowId5Tuple& other) const {
        return src_ip == other.src_ip && dst_ip == other.dst_ip &&
               src_port == other.src_port && dst_port == other.dst_port &&
               protocol == other.protocol;
    }
};
} // namespace core
} // namespace hqts

namespace std {
template <>
struct hash<hqts::core::FlowId5Tuple> {
    size_t operator()(const hqts::core::FlowId5Tuple& k) const {
        // A common way to combine hashes:
        size_t h1 = std::hash<uint32_t>{}(k.src_ip);
        size_t h2 = std::hash<uint32_t>{}(k.dst_ip);
        size_t h3 = std::hash<uint16_t>{}(k.src_port);
        size_t h4 = std::hash<uint16_t>{}(k.dst_port);
        size_t h5 = std::hash<uint8_t>{}(k.protocol);
        // Simple XOR combination (can be improved)
        return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
    }
};
} // namespace std
*/

using FlowTable = std::unordered_map<core::FlowId, core::FlowContext>;

} // namespace dataplane
} // namespace hqts

#endif // HQTS_DATAPLANE_FLOW_TABLE_H_
