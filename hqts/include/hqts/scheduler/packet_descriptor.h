#ifndef HQTS_SCHEDULER_PACKET_DESCRIPTOR_H_
#define HQTS_SCHEDULER_PACKET_DESCRIPTOR_H_

#include "hqts/core/flow_context.h" // For hqts::core::FlowId

#include <cstdint>
#include <vector>
#include <cstddef> // For std::byte (if C++17) or alternative

// Ensure std::byte is available (C++17 and later)
#if __cplusplus >= 201703L
#include <cstddef> // For std::byte
#else
// Basic fallback for pre-C++17, not a perfect polyfill but enough for opaque data
namespace std {
enum class byte : unsigned char {};
}
#endif


namespace hqts {
namespace scheduler {

// Defines the conformance level of a packet after policing/shaping.
enum class ConformanceLevel {
    GREEN,  // Conforms to CIR (Committed Information Rate)
    YELLOW, // Exceeds CIR, but conforms to PIR (Peak Information Rate)
    RED     // Exceeds PIR
};

struct PacketDescriptor {
    core::FlowId flow_id;
    uint32_t packet_length_bytes;
    uint8_t priority; // This will be set by TrafficShaper based on policy conformance outcomes
                      // Schedulers (like StrictPriority or WRR using priority as QueueId) will use this.
    ConformanceLevel conformance; // Set by the shaper/policer

    // Optional: Represents the packet payload or a reference to it.
    std::vector<std::byte> payload;

    // Constructor
    PacketDescriptor(
        core::FlowId f_id,
        uint32_t len,
        uint8_t prio_val = 0, // Default priority, to be overwritten by shaper
        size_t actual_payload_to_copy_size = 0,
        ConformanceLevel conf = ConformanceLevel::GREEN // Default conformance
    ) : flow_id(f_id),
        packet_length_bytes(len),
        priority(prio_val),
        conformance(conf) {
        if (actual_payload_to_copy_size > 0) {
            payload.resize(actual_payload_to_copy_size);
        }
    }

    // Default constructor for cases where it might be needed
    PacketDescriptor()
      : flow_id(0),
        packet_length_bytes(0),
        priority(0),
        conformance(ConformanceLevel::GREEN),
        payload{} {}
};

} // namespace scheduler
} // namespace hqts

#endif // HQTS_SCHEDULER_PACKET_DESCRIPTOR_H_
