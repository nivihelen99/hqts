#include "hqts/core/packet_pipeline.h"

// Full includes for implementations
#include "hqts/dataplane/flow_classifier.h"
#include "hqts/core/traffic_shaper.h"
#include "hqts/scheduler/scheduler_interface.h"
// scheduler/packet_descriptor.h and dataplane/flow_identifier.h are included by packet_pipeline.h
// vector and cstddef are also included by packet_pipeline.h

namespace hqts {
namespace core {

PacketPipeline::PacketPipeline(
    dataplane::FlowClassifier& classifier,
    TrafficShaper& shaper,
    scheduler::SchedulerInterface& scheduler)
    : classifier_(classifier), shaper_(shaper), scheduler_(scheduler) {
    // Constructor body, if any initialization beyond member list is needed
}

void PacketPipeline::handle_incoming_packet(
    const dataplane::FiveTuple& five_tuple,
    uint32_t packet_length_bytes,
    const std::vector<std::byte>& payload) {

    // 1. Create a PacketDescriptor.
    // Initial FlowId and priority are set to dummy values (e.g., 0).
    // TrafficShaper will set the correct packet.flow_id after classification
    // and packet.priority based on policy and conformance.
    // The PacketDescriptor constructor handles payload_size for internal vector resize.
    scheduler::PacketDescriptor packet(
        0, // Initial dummy flow_id
        packet_length_bytes,
        0, // Initial dummy priority
        payload.size() // Provide payload size for potential internal vector allocation
    );
    if (!payload.empty()) {
        packet.payload = payload; // Copy payload if provided and necessary
    }

    // 2. Process through TrafficShaper.
    // TrafficShaper::process_packet will:
    //   a. Use FlowClassifier to get/create FlowId and associated FlowContext.
    //   b. Set packet.flow_id.
    //   c. Apply token buckets from the policy.
    //   d. Set packet.conformance (GREEN, YELLOW, RED).
    //   e. Set packet.priority based on conformance and policy targets.
    //   f. Return true if packet should be enqueued, false if dropped by policy.
    bool should_enqueue = shaper_.process_packet(packet, five_tuple);

    // 3. Enqueue if not dropped.
    if (should_enqueue) {
        scheduler_.enqueue(std::move(packet)); // Move packet into the scheduler
    } else {
        // Packet was dropped by the shaper (due to policy, e.g., RED and drop_on_red=true).
        // Action: Log, increment drop counter, etc. (Not implemented here)
    }
}

scheduler::PacketDescriptor PacketPipeline::get_next_packet_to_transmit() {
    if (!scheduler_.is_empty()) {
        return scheduler_.dequeue();
    }
    // Return a default-constructed PacketDescriptor if the scheduler is empty.
    // The default PacketDescriptor constructor sets flow_id=0, length=0, etc.
    // which can be checked by the caller to see if it's a valid packet.
    return scheduler::PacketDescriptor();
}

} // namespace core
} // namespace hqts
