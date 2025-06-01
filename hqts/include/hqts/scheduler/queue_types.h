#ifndef HQTS_SCHEDULER_QUEUE_TYPES_H_
#define HQTS_SCHEDULER_QUEUE_TYPES_H_

#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor

#include <queue>    // For std::queue
#include <deque>    // std::queue is often implemented using std::deque by default
#include <vector>   // Can also be used as std::queue's underlying container
#include <list>     // Can also be used as std::queue's underlying container

namespace hqts {
namespace scheduler {

// Basic packet queue type using std::queue with std::deque as the underlying container.
// std::deque is generally a good default for std::queue.
using PacketQueue = std::queue<PacketDescriptor>;

// Alternative queue types could be defined here if needed, for example:
// using PriorityPacketQueue = std::priority_queue<PacketDescriptor, std::vector<PacketDescriptor>, SomeComparator>;
// using BoundedPacketQueue = ... (would require a custom bounded queue implementation)

} // namespace scheduler
} // namespace hqts

#endif // HQTS_SCHEDULER_QUEUE_TYPES_H_
