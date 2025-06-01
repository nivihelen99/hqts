#ifndef HQTS_CORE_PACKET_PIPELINE_H_
#define HQTS_CORE_PACKET_PIPELINE_H_

#include "hqts/dataplane/flow_identifier.h"   // For dataplane::FiveTuple
#include "hqts/scheduler/packet_descriptor.h" // For scheduler::PacketDescriptor

#include <vector>   // For std::vector
#include <cstddef>  // For std::byte
// memory for std::unique_ptr or shared_ptr is not used in this header's declarations

// Forward declarations to minimize header include dependencies
namespace hqts {
namespace dataplane { class FlowClassifier; }
namespace core { class TrafficShaper; } // TrafficShaper is in hqts::core
namespace scheduler { class SchedulerInterface; }
} // namespace hqts


namespace hqts {
namespace core {

class PacketPipeline {
public:
    /**
     * @brief Constructor for PacketPipeline.
     * @param classifier Reference to the FlowClassifier instance.
     * @param shaper Reference to the TrafficShaper instance.
     * @param scheduler Reference to the SchedulerInterface instance.
     */
    PacketPipeline(
        dataplane::FlowClassifier& classifier,
        TrafficShaper& shaper, // TrafficShaper is in hqts::core
        scheduler::SchedulerInterface& scheduler);

    // PacketPipeline is stateful via its references, make it non-copyable/non-movable
    // if it's intended to be a long-lived service object.
    PacketPipeline(const PacketPipeline&) = delete;
    PacketPipeline& operator=(const PacketPipeline&) = delete;
    PacketPipeline(PacketPipeline&&) = delete;
    PacketPipeline& operator=(PacketPipeline&&) = delete;


    /**
     * @brief Simulates handling of an incoming raw packet.
     * The method creates a PacketDescriptor, classifies the flow, applies shaping policies,
     * and if the packet is not dropped by the shaper, enqueues it into the scheduler.
     * @param five_tuple The 5-tuple identifying the packet's flow.
     * @param packet_length_bytes The total length of the packet in bytes.
     * @param payload Optional payload data for the packet.
     */
    void handle_incoming_packet(const dataplane::FiveTuple& five_tuple,
                                uint32_t packet_length_bytes,
                                const std::vector<std::byte>& payload = {});

    /**
     * @brief Retrieves the next packet to be "transmitted" from the scheduler.
     * @return A PacketDescriptor for the next packet. If the scheduler is empty,
     *         a default-constructed (e.g., invalid) PacketDescriptor is returned.
     */
    scheduler::PacketDescriptor get_next_packet_to_transmit();

private:
    dataplane::FlowClassifier& classifier_;
    TrafficShaper& shaper_; // TrafficShaper is in hqts::core
    scheduler::SchedulerInterface& scheduler_;
};

} // namespace core
} // namespace hqts

#endif // HQTS_CORE_PACKET_PIPELINE_H_
