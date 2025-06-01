#ifndef HQTS_DATAPLANE_FLOW_IDENTIFIER_H_
#define HQTS_DATAPLANE_FLOW_IDENTIFIER_H_

#include <cstdint>
#include <string>     // For potential future use (e.g. string IPs if needed, though not current)
#include <functional> // For std::hash
#include <utility>    // For std::pair (not directly used here, but often with hashing)

// Forward declaration of core::FlowId is not strictly necessary here as this file
// defines the key structure used to *look up or generate* a core::FlowId.
// The actual core::FlowId (uint64_t) is defined in hqts/core/flow_context.h.

namespace hqts {
namespace dataplane {

// Basic 5-tuple structure for flow identification.
// In a real system, IP addresses might be represented by more complex types
// (e.g., from a networking library, or structs that handle IPv4/IPv6).
struct FiveTuple {
    uint32_t source_ip;      // IPv4 source address
    uint32_t dest_ip;        // IPv4 destination address
    uint16_t source_port;    // Source port (TCP/UDP)
    uint16_t dest_port;      // Destination port (TCP/UDP)
    uint8_t protocol;       // IP protocol (e.g., IPPROTO_TCP, IPPROTO_UDP from netinet/in.h)

    // Constructor with default values
    FiveTuple(uint32_t s_ip = 0, uint32_t d_ip = 0,
              uint16_t s_port = 0, uint16_t d_port = 0,
              uint8_t proto = 0)
        : source_ip(s_ip), dest_ip(d_ip),
          source_port(s_port), dest_port(d_port), protocol(proto) {}

    // Equality operator: crucial for std::unordered_map to handle hash collisions.
    bool operator==(const FiveTuple& other) const {
        return source_ip == other.source_ip &&
               dest_ip == other.dest_ip &&
               source_port == other.source_port &&
               dest_port == other.dest_port &&
               protocol == other.protocol;
    }

    // Optional: A less-than operator can be useful for ordered maps or sets,
    // though not strictly required for std::unordered_map.
    bool operator<(const FiveTuple& other) const {
        if (source_ip != other.source_ip) return source_ip < other.source_ip;
        if (dest_ip != other.dest_ip) return dest_ip < other.dest_ip;
        if (source_port != other.source_port) return source_port < other.source_port;
        if (dest_port != other.dest_port) return dest_port < other.dest_port;
        return protocol < other.protocol;
    }
};

// FlowKey is defined as the FiveTuple for identifying flows based on packet headers.
using FlowKey = FiveTuple;

// Note: The core::FlowId (defined in hqts/core/flow_context.h as uint64_t)
// will be the actual identifier used as the key in the FlowTable.
// A FlowClassifier component will be responsible for mapping a FlowKey (FiveTuple)
// derived from a packet to a unique core::FlowId (e.g., by hashing the FlowKey
// or using a monotonic counter for new flows).

} // namespace dataplane
} // namespace hqts

// Specialization of std::hash for hqts::dataplane::FiveTuple.
// This allows hqts::dataplane::FiveTuple (and thus FlowKey) to be used as a key
// in std::unordered_map and std::unordered_set.
namespace std {
template <>
struct hash<hqts::dataplane::FiveTuple> {
    size_t operator()(const hqts::dataplane::FiveTuple& k) const {
        // A common way to combine hashes for multiple fields.
        // This specific sequence is often used (similar to Boost's hash_combine).
        size_t seed = 0;
        seed ^= std::hash<uint32_t>{}(k.source_ip) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint32_t>{}(k.dest_ip) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint16_t>{}(k.source_port) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint16_t>{}(k.dest_port) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint8_t>{}(k.protocol) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};
} // namespace std

#endif // HQTS_DATAPLANE_FLOW_IDENTIFIER_H_
