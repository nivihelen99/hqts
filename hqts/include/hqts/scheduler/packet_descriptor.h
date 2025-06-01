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

struct PacketDescriptor {
    core::FlowId flow_id;
    uint32_t packet_length_bytes;
    uint8_t priority; // Could be derived from flow's policy (e.g., DSCP, PCP)

    // Optional: Represents the packet payload or a reference to it.
    // For a descriptor, it might not store the full payload but metadata or a pointer.
    // Using std::vector<std::byte> for an owned, opaque payload for simplicity here.
    std::vector<std::byte> payload;

    // Constructor
    PacketDescriptor(core::FlowId f_id, uint32_t len, uint8_t prio, size_t actual_payload_to_copy_size = 0)
        : flow_id(f_id), packet_length_bytes(len), priority(prio) {
        if (actual_payload_to_copy_size > 0) {
            // This constructor implies copying at least part of a payload if given.
            // In a real system, payload might be a pointer/reference or handled differently.
            // For now, we just resize to simulate holding some data.
            payload.resize(actual_payload_to_copy_size);
            // In a real scenario, one might do:
            // if (source_payload_ptr && actual_payload_to_copy_size > 0) {
            //    payload.assign(static_cast<const std::byte*>(source_payload_ptr),
            //                   static_cast<const std::byte*>(source_payload_ptr) + actual_payload_to_copy_size);
            // }
        }
    }

    // Default constructor for cases where it might be needed
    PacketDescriptor() = default;
};

} // namespace scheduler
} // namespace hqts

#endif // HQTS_SCHEDULER_PACKET_DESCRIPTOR_H_
