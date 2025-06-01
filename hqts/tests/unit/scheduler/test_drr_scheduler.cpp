#include "gtest/gtest.h"
#include "hqts/scheduler/drr_scheduler.h"
#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor
#include "hqts/core/flow_context.h"          // For core::QueueId, core::FlowId
#include "hqts/scheduler/aqm_queue.h"        // For RedAqmParameters

#include <vector>
#include <map>
#include <numeric>   // For std::accumulate if needed
#include <stdexcept> // For std::invalid_argument etc.

namespace hqts {
namespace scheduler {

// Helper to create a PacketDescriptor. Priority field is used as QueueId for DRR.
PacketDescriptor createDrrTestPacket(core::FlowId flow_id, uint32_t length, core::QueueId queue_id_for_drr) {
    if (queue_id_for_drr > UINT8_MAX) {
        throw std::overflow_error("Queue ID for DRR test packet exceeds uint8_t max value.");
    }
    return PacketDescriptor(flow_id, length, static_cast<uint8_t>(queue_id_for_drr));
}

// Helper to create a vector of permissive RedAqmParameters for DRR queues
std::vector<DrrScheduler::QueueConfig> createDrrConfigsWithPermissiveAqm(
    const std::vector<std::pair<core::QueueId, uint32_t>>& queue_defs,
    uint32_t capacity_bytes = 1000000,
    double max_p = 0.001,
    double ewma_w = 0.002) {

    std::vector<DrrScheduler::QueueConfig> configs;
    for (const auto& q_def : queue_defs) {
        RedAqmParameters aqm_params(
            static_cast<uint32_t>(capacity_bytes * 0.8),
            static_cast<uint32_t>(capacity_bytes * 0.9),
            max_p, ewma_w, capacity_bytes
        );
        configs.emplace_back(q_def.first, q_def.second, aqm_params);
    }
    return configs;
}


TEST(DrrSchedulerTest, ConstructorValidation) {
    std::vector<DrrScheduler::QueueConfig> empty_configs;
    ASSERT_THROW(DrrScheduler scheduler(empty_configs), std::invalid_argument);

    RedAqmParameters permissive_aqm = RedAqmParameters(1000,2000,0.1,0.1,3000);

    std::vector<DrrScheduler::QueueConfig> zero_quantum_configs = {{static_cast<core::QueueId>(1), 0, permissive_aqm}};
    ASSERT_THROW(DrrScheduler scheduler(zero_quantum_configs), std::invalid_argument);

    std::vector<DrrScheduler::QueueConfig> zero_quantum_configs2 = {
        {static_cast<core::QueueId>(1), 100, permissive_aqm},
        {static_cast<core::QueueId>(2), 0, permissive_aqm}
    };
    ASSERT_THROW(DrrScheduler scheduler(zero_quantum_configs2), std::invalid_argument);

    std::vector<DrrScheduler::QueueConfig> duplicate_id_configs = {
        {static_cast<core::QueueId>(1), 100, permissive_aqm},
        {static_cast<core::QueueId>(1), 200, permissive_aqm}
    };
    ASSERT_THROW(DrrScheduler scheduler(duplicate_id_configs), std::invalid_argument);

    std::vector<DrrScheduler::QueueConfig> valid_configs = {
        {static_cast<core::QueueId>(1), 100, permissive_aqm},
        {static_cast<core::QueueId>(2), 200, permissive_aqm}
    };
    ASSERT_NO_THROW(DrrScheduler scheduler(valid_configs));
    DrrScheduler scheduler(valid_configs);
    ASSERT_EQ(scheduler.get_num_queues(), 2);
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(DrrSchedulerTest, EnqueueAndDequeueSinglePacket) {
    auto configs = createDrrConfigsWithPermissiveAqm({{static_cast<core::QueueId>(100), 500}});
    DrrScheduler scheduler(configs);

    PacketDescriptor p1 = createDrrTestPacket(1, 100, 100);
    scheduler.enqueue(p1); // Permissive AQM should accept
    ASSERT_FALSE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(100), 1);

    PacketDescriptor dequeued_p = scheduler.dequeue();
    ASSERT_EQ(dequeued_p.flow_id, p1.flow_id);
    ASSERT_EQ(dequeued_p.packet_length_bytes, p1.packet_length_bytes);
    ASSERT_EQ(static_cast<core::QueueId>(dequeued_p.priority), static_cast<core::QueueId>(100));
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_queue_size(100), 0);
}

TEST(DrrSchedulerTest, DequeueFromEmpty) {
    auto configs = createDrrConfigsWithPermissiveAqm({{static_cast<core::QueueId>(1), 100}});
    DrrScheduler scheduler(configs);
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_THROW(scheduler.dequeue(), std::runtime_error);
}

TEST(DrrSchedulerTest, EnqueueInvalidQueueId) {
    auto configs = createDrrConfigsWithPermissiveAqm({{static_cast<core::QueueId>(1), 100}});
    DrrScheduler scheduler(configs);
    PacketDescriptor p_valid = createDrrTestPacket(1, 50, 1);
    PacketDescriptor p_invalid_q_id = createDrrTestPacket(2, 100, 2);

    ASSERT_NO_THROW(scheduler.enqueue(p_valid));
    ASSERT_THROW(scheduler.enqueue(p_invalid_q_id), std::out_of_range);
}

TEST(DrrSchedulerTest, BasicFairnessEqualSizePackets) {
    auto configs = createDrrConfigsWithPermissiveAqm({
        {static_cast<core::QueueId>(1), 100}, {static_cast<core::QueueId>(2), 100}
    });
    DrrScheduler scheduler(configs);

    for (int i = 0; i < 5; ++i) scheduler.enqueue(createDrrTestPacket(10 + i, 100, 1));
    for (int i = 0; i < 5; ++i) scheduler.enqueue(createDrrTestPacket(20 + i, 100, 2));

    std::map<core::QueueId, int> dequeued_counts;
    for (int i = 0; i < 10; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        dequeued_counts[static_cast<core::QueueId>(p.priority)]++;
    }
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(1)], 5);
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(2)], 5);
}

TEST(DrrSchedulerTest, FairnessDifferentQuantumsEqualSizePackets) {
    auto configs = createDrrConfigsWithPermissiveAqm({
        {static_cast<core::QueueId>(1), 100}, {static_cast<core::QueueId>(2), 200}
    });
    DrrScheduler scheduler(configs);
    for (int i = 0; i < 3; ++i) scheduler.enqueue(createDrrTestPacket(10 + i, 100, 1));
    for (int i = 0; i < 6; ++i) scheduler.enqueue(createDrrTestPacket(20 + i, 100, 2));

    std::map<core::QueueId, int> dequeued_counts;
    for (int i = 0; i < 9; ++i) {
        dequeued_counts[static_cast<core::QueueId>(scheduler.dequeue().priority)]++;
    }
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(1)], 3);
    ASSERT_EQ(dequeued_counts[static_cast<core::QueueId>(2)], 6);
}

TEST(DrrSchedulerTest, FairnessVariablePacketSizes) {
    auto configs = createDrrConfigsWithPermissiveAqm({
        {static_cast<core::QueueId>(1), 300}, {static_cast<core::QueueId>(2), 300}
    });
    DrrScheduler scheduler(configs);
    for (int i = 0; i < 6; ++i) scheduler.enqueue(createDrrTestPacket(10 + i, 50, 1));
    for (int i = 0; i < 2; ++i) scheduler.enqueue(createDrrTestPacket(20 + i, 150, 2));

    std::map<core::QueueId, uint32_t> dequeued_byte_counts;
    std::map<core::QueueId, int> dequeued_packet_counts;
    for (int i = 0; i < 8; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        dequeued_byte_counts[static_cast<core::QueueId>(p.priority)] += p.packet_length_bytes;
        dequeued_packet_counts[static_cast<core::QueueId>(p.priority)]++;
    }
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(dequeued_packet_counts[static_cast<core::QueueId>(1)], 6);
    ASSERT_EQ(dequeued_byte_counts[static_cast<core::QueueId>(1)], 300);
    ASSERT_EQ(dequeued_packet_counts[static_cast<core::QueueId>(2)], 2);
    ASSERT_EQ(dequeued_byte_counts[static_cast<core::QueueId>(2)], 300);
}

TEST(DrrSchedulerTest, DeficitHandlingLargePacket) {
    auto configs = createDrrConfigsWithPermissiveAqm({
        {static_cast<core::QueueId>(1), 100}, {static_cast<core::QueueId>(2), 100}
    });
    DrrScheduler scheduler(configs);
    scheduler.enqueue(createDrrTestPacket(1, 250, 1));
    scheduler.enqueue(createDrrTestPacket(101, 10, 2));
    scheduler.enqueue(createDrrTestPacket(102, 10, 2));
    scheduler.enqueue(createDrrTestPacket(103, 10, 2));

    ASSERT_EQ(scheduler.dequeue().flow_id, 101);
    ASSERT_EQ(scheduler.dequeue().flow_id, 102);
    ASSERT_EQ(scheduler.dequeue().flow_id, 1);
    ASSERT_EQ(scheduler.dequeue().flow_id, 103);
    ASSERT_TRUE(scheduler.is_empty());
}

// --- New Tests for AQM Behavior with DRR ---

TEST(DrrSchedulerAqmTest, AqmDropInDrrQueue) {
    core::QueueId qid_normal = 0;
    core::QueueId qid_lossy = 1;
    uint32_t normal_quantum = 1000;
    uint32_t lossy_quantum = 1000;

    RedAqmParameters permissive_params = RedAqmParameters(8000, 9000, 0.01, 0.002, 10000);
    // Aggressive AQM: min_thresh=100, max_thresh=200, capacity=300 bytes, max_p=0.5, w=1.0
    RedAqmParameters aggressive_params = RedAqmParameters(100, 200, 0.5, 1.0, 300);

    std::vector<DrrScheduler::QueueConfig> configs = {
        {qid_normal, normal_quantum, permissive_params},
        {qid_lossy, lossy_quantum, aggressive_params}
    };
    DrrScheduler scheduler(configs);

    int normal_queue_expected_count = 5;
    for (int i = 0; i < normal_queue_expected_count; ++i) {
        scheduler.enqueue(createDrrTestPacket(i, 100, qid_normal));
    }
    ASSERT_EQ(scheduler.get_queue_size(qid_normal), normal_queue_expected_count);

    size_t initial_total_packets = 0;
    for(size_t i=0; i < scheduler.get_num_queues(); ++i) initial_total_packets += scheduler.get_queue_size(static_cast<uint8_t>(i));


    int attempts_on_lossy_queue = 50; // Enough to trigger RED drops
    for (int i = 0; i < attempts_on_lossy_queue; ++i) {
        scheduler.enqueue(createDrrTestPacket(100 + i, 10, qid_lossy)); // Small packets
    }

    size_t lossy_queue_actual_count = scheduler.get_queue_size(qid_lossy);
    ASSERT_LT(lossy_queue_actual_count, attempts_on_lossy_queue);
    ASSERT_GT(lossy_queue_actual_count, 0);

    size_t total_packets_actual = 0;
    for(size_t i=0; i < scheduler.get_num_queues(); ++i) total_packets_actual += scheduler.get_queue_size(static_cast<uint8_t>(i));

    ASSERT_EQ(total_packets_actual, normal_queue_expected_count + lossy_queue_actual_count);

    // Dequeue all and verify counts. Order depends on DRR and what was accepted.
    std::map<core::QueueId, int> final_counts;
    while(!scheduler.is_empty()){
        final_counts[static_cast<core::QueueId>(scheduler.dequeue().priority)]++;
    }
    ASSERT_EQ(final_counts[qid_normal], normal_queue_expected_count);
    ASSERT_EQ(final_counts[qid_lossy], lossy_queue_actual_count);
}

TEST(DrrSchedulerAqmTest, AqmPhysicalCapacityDropInDrrQueue) {
    core::QueueId qid_small_cap = 0;
    core::QueueId qid_normal_cap = 1;

    RedAqmParameters small_cap_params(80, 90, 0.1, 0.002, 100); // Prio 0: Small capacity 100 bytes
    RedAqmParameters normal_cap_params(800, 900, 0.1, 0.002, 1000); // Prio 1: Larger capacity

    std::vector<DrrScheduler::QueueConfig> configs = {
        {qid_small_cap, 1000, small_cap_params},
        {qid_normal_cap, 1000, normal_cap_params}
    };
    DrrScheduler scheduler(configs);

    scheduler.enqueue(createDrrTestPacket(1, 50, qid_small_cap));
    scheduler.enqueue(createDrrTestPacket(2, 50, qid_small_cap));
    ASSERT_EQ(scheduler.get_queue_size(qid_small_cap), 2);

    size_t total_pkts_before_drop_attempt = scheduler.get_queue_size(qid_small_cap) + scheduler.get_queue_size(qid_normal_cap);
    scheduler.enqueue(createDrrTestPacket(3, 10, qid_small_cap)); // This should be dropped by physical capacity
    size_t total_pkts_after_drop_attempt = scheduler.get_queue_size(qid_small_cap) + scheduler.get_queue_size(qid_normal_cap);

    ASSERT_EQ(total_pkts_before_drop_attempt, total_pkts_after_drop_attempt);
    ASSERT_EQ(scheduler.get_queue_size(qid_small_cap), 2);

    scheduler.enqueue(createDrrTestPacket(4, 100, qid_normal_cap));
    ASSERT_EQ(scheduler.get_queue_size(qid_normal_cap), 1);
}


} // namespace scheduler
} // namespace hqts
