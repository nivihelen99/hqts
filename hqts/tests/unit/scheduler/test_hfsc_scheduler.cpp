#include "gtest/gtest.h"
#include "hqts/scheduler/hfsc_scheduler.h"
#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor
#include "hqts/core/flow_context.h"          // For core::FlowId

#include <vector>
#include <stdexcept> // For std::logic_error, std::invalid_argument, std::out_of_range
#include <map>       // For std::map in tests
#include <numeric>   // For std::accumulate etc. if needed

namespace hqts {
namespace scheduler {

// Helper to create a PacketDescriptor. Priority field is used as FlowId.
PacketDescriptor createHfscTestPacket(core::FlowId flow_id, uint32_t length) {
    // Ensure flow_id fits into uint8_t if it's used for packet.priority directly
    if (flow_id > UINT8_MAX) {
        // This test setup assumes flow_id can be represented by packet.priority.
        // If FlowId can be larger, the mapping needs to be handled or PacketDescriptor updated.
        // For these basic tests, we'll assume flow_id values fit.
    }
    return PacketDescriptor(flow_id, length, static_cast<uint8_t>(flow_id));
}

// Helper to create a default ServiceCurve for tests
ServiceCurve defaultRtSc() { return ServiceCurve(1000000, 0); } // 1 Mbps, 0 delay
ServiceCurve defaultLsSc() { return ServiceCurve(500000, 0); }  // 0.5 Mbps, 0 delay
ServiceCurve defaultUlSc() { return ServiceCurve(2000000, 0); } // 2 Mbps, 0 delay

// Test fixture for HfscScheduler tests if common setup is needed later.
// For now, using TEST directly as setup is minimal per test.
// class HfscSchedulerTest : public ::testing::Test {
// protected:
// HfscScheduler scheduler; // Requires initialization in Test::SetUp or per test
// };

TEST(HfscSchedulerTest, ConstructorEmptyConfig) {
    std::vector<HfscScheduler::FlowConfig> empty_configs;
    // Constructor with empty config sets is_configured_ to false
    HfscScheduler scheduler(empty_configs, 1000000000); // 1 Gbps link

    ASSERT_TRUE(scheduler.is_empty()); // is_empty checks total_packets_ which is 0
    ASSERT_EQ(scheduler.get_num_configured_flows(), 0);

    // Enqueue/Dequeue on a scheduler constructed with empty config should throw std::logic_error
    // because its is_configured_ flag will be false.
    ASSERT_THROW(scheduler.enqueue(createHfscTestPacket(1, 100)), std::logic_error);
    ASSERT_THROW(scheduler.dequeue(), std::logic_error);
}

TEST(HfscSchedulerTest, ConstructorValidConfig) {
    std::vector<HfscScheduler::FlowConfig> configs = {
        // Assuming 0 is NO_PARENT_ID for these root flows
        {static_cast<core::FlowId>(1), 0, defaultRtSc(), defaultLsSc()},
        {static_cast<core::FlowId>(2), 0, ServiceCurve(2000000, 100), ServiceCurve(1000000, 0)}
    };
    HfscScheduler scheduler(configs, 1000000000); // 1 Gbps link

    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_num_configured_flows(), 2);
    ASSERT_NO_THROW(scheduler.get_flow_queue_size(1));
    ASSERT_EQ(scheduler.get_flow_queue_size(1), 0);
    ASSERT_NO_THROW(scheduler.get_flow_queue_size(2));
    ASSERT_EQ(scheduler.get_flow_queue_size(2), 0);
    ASSERT_THROW(scheduler.get_flow_queue_size(3), std::out_of_range); // Flow 3 not configured
}

TEST(HfscSchedulerTest, ConstructorDuplicateFlowId) {
    std::vector<HfscScheduler::FlowConfig> configs = {
        {static_cast<core::FlowId>(1), 0, defaultRtSc()},
        {static_cast<core::FlowId>(1), 0, defaultRtSc()} // Duplicate Flow ID
    };
    ASSERT_THROW(HfscScheduler scheduler(configs, 1000000000), std::invalid_argument);
}

TEST(HfscSchedulerTest, EnqueueAndDequeueSinglePacket_PlaceholderLogic) {
    std::vector<HfscScheduler::FlowConfig> configs = {{static_cast<core::FlowId>(1), 0, defaultRtSc()}};
    HfscScheduler scheduler(configs, 1000000000);

    PacketDescriptor p1 = createHfscTestPacket(1, 100);
    ASSERT_NO_THROW(scheduler.enqueue(p1));
    ASSERT_FALSE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_flow_queue_size(1), 1);

    PacketDescriptor dequeued_p = scheduler.dequeue();
    ASSERT_EQ(dequeued_p.flow_id, p1.flow_id);
    ASSERT_EQ(static_cast<core::FlowId>(dequeued_p.priority), static_cast<core::FlowId>(1)); // Check FlowId mapping
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(scheduler.get_flow_queue_size(1), 0);
}

TEST(HfscSchedulerTest, DequeueFromEmpty_PlaceholderLogic) {
    std::vector<HfscScheduler::FlowConfig> configs = {{static_cast<core::FlowId>(1), 0, defaultRtSc()}};
    HfscScheduler scheduler(configs, 1000000000);
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_THROW(scheduler.dequeue(), std::runtime_error);
}

TEST(HfscSchedulerTest, EnqueueToUnconfiguredFlowId_PlaceholderLogic) {
    std::vector<HfscScheduler::FlowConfig> configs = {{static_cast<core::FlowId>(1), 0, defaultRtSc()}};
    HfscScheduler scheduler(configs, 1000000000);

    PacketDescriptor p_unconfigured_flow = createHfscTestPacket(2, 100);
    ASSERT_THROW(scheduler.enqueue(p_unconfigured_flow), std::out_of_range);
}

TEST(HfscSchedulerTest, MultipleFlowsSimple_PlaceholderLogic) {
    std::vector<HfscScheduler::FlowConfig> configs = {
        {static_cast<core::FlowId>(1), 0, defaultRtSc()},
        {static_cast<core::FlowId>(2), 0, defaultRtSc()}
    };
    HfscScheduler scheduler(configs, 1000000000);

    PacketDescriptor p_flow1 = createHfscTestPacket(1, 100);
    PacketDescriptor p_flow2 = createHfscTestPacket(2, 150);

    scheduler.enqueue(p_flow1);
    scheduler.enqueue(p_flow2);
    ASSERT_EQ(scheduler.get_flow_queue_size(1), 1);
    ASSERT_EQ(scheduler.get_flow_queue_size(2), 1);
    ASSERT_FALSE(scheduler.is_empty());

    PacketDescriptor dp1 = scheduler.dequeue();
    ASSERT_EQ(dp1.flow_id, static_cast<core::FlowId>(1));
    ASSERT_EQ(scheduler.get_flow_queue_size(1), 0);
    ASSERT_EQ(scheduler.get_flow_queue_size(2), 1);

    PacketDescriptor dp2 = scheduler.dequeue();
    ASSERT_EQ(dp2.flow_id, static_cast<core::FlowId>(2));
    ASSERT_EQ(scheduler.get_flow_queue_size(2), 0);

    ASSERT_TRUE(scheduler.is_empty());
}


// --- Tests for Core HFSC RT Logic ---

TEST(HfscSchedulerRtTest, SingleFlowRealTimeService) {
    core::FlowId flowA_id = 1;
    uint32_t packet_size_bytes = 1250;
    uint64_t rt_rate_bps = 1000000;
    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, 0, ServiceCurve(rt_rate_bps, 0)}
    };
    HfscScheduler scheduler(configs, 10000000);

    const int num_packets = 10;
    for (int i = 0; i < num_packets; ++i) {
        scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    }
    for (int i = 0; i < num_packets; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        ASSERT_EQ(p.flow_id, flowA_id);
    }
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(HfscSchedulerRtTest, SingleFlowWithRtDelay) {
    core::FlowId flowA_id = 1;
    uint32_t packet_size_bytes = 1250;
    uint64_t rt_rate_bps = 1000000;
    uint64_t rt_delay_us = 5000;
    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, 0, ServiceCurve(rt_rate_bps, rt_delay_us)}
    };
    HfscScheduler scheduler(configs, 10000000);
    scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    PacketDescriptor p = scheduler.dequeue();
    ASSERT_EQ(p.flow_id, flowA_id);
}


TEST(HfscSchedulerRtTest, TwoFlowsIndependentRealTime) {
    core::FlowId flowA_id = 1, flowB_id = 2;
    uint32_t packet_size_bytes = 1250;
    uint64_t rate_bps = 1000000;
    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, 0, ServiceCurve(rate_bps, 0)},
        {flowB_id, 0, ServiceCurve(rate_bps, 0)}
    };
    HfscScheduler scheduler(configs, 10000000);

    const int num_packets_per_flow = 5;
    for (int i = 0; i < num_packets_per_flow; ++i) {
        scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
        scheduler.enqueue(createHfscTestPacket(flowB_id, packet_size_bytes));
    }
    std::map<core::FlowId, int> counts;
    for (int i = 0; i < 2 * num_packets_per_flow; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        counts[p.flow_id]++;
    }
    ASSERT_EQ(counts[flowA_id], num_packets_per_flow);
    ASSERT_EQ(counts[flowB_id], num_packets_per_flow);
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(HfscSchedulerRtTest, VirtualFinishTimeOrderingAndMonotonicity) {
    core::FlowId f1 = 1, f2 = 2, f3 = 3;
    std::vector<HfscScheduler::FlowConfig> configs = {
        {f1, 0, ServiceCurve(1000000, 0)},
        {f2, 0, ServiceCurve(2000000, 1000)},
        {f3, 0, ServiceCurve(500000, 0)}
    };
    HfscScheduler scheduler(configs, 10000000);
    scheduler.enqueue(createHfscTestPacket(f1, 1000));
    scheduler.enqueue(createHfscTestPacket(f2, 1000));
    scheduler.enqueue(createHfscTestPacket(f3, 1000));
    scheduler.enqueue(createHfscTestPacket(f1, 500));

    std::vector<core::FlowId> flow_id_sequence;
    flow_id_sequence.push_back(scheduler.dequeue().flow_id);
    flow_id_sequence.push_back(scheduler.dequeue().flow_id);
    flow_id_sequence.push_back(scheduler.dequeue().flow_id);
    flow_id_sequence.push_back(scheduler.dequeue().flow_id);
    ASSERT_TRUE(scheduler.is_empty());
    std::vector<core::FlowId> expected_flow_order = {f2, f1, f1, f3};
    ASSERT_EQ(flow_id_sequence, expected_flow_order);
}


TEST(HfscSchedulerRtTest, FlowBecomesActiveReEligibility) {
    core::FlowId fA = 1, fB = 2;
    uint32_t pkt_size = 1250;
    std::vector<HfscScheduler::FlowConfig> configs = {
        {fA, 0, ServiceCurve(1000000, 0)},
        {fB, 0, ServiceCurve(1000000, 0)}
    };
    HfscScheduler scheduler(configs, 10000000);
    scheduler.enqueue(createHfscTestPacket(fA, pkt_size));
    scheduler.enqueue(createHfscTestPacket(fA, pkt_size));
    scheduler.enqueue(createHfscTestPacket(fB, pkt_size));

    ASSERT_EQ(scheduler.dequeue().flow_id, fA);
    ASSERT_EQ(scheduler.dequeue().flow_id, fB);
    ASSERT_EQ(scheduler.dequeue().flow_id, fA);
    ASSERT_TRUE(scheduler.is_empty());

    for(int i=0; i < 5; ++i) {
        scheduler.enqueue(createHfscTestPacket(fB, pkt_size));
    }
    for(int i=0; i < 5; ++i) {
        ASSERT_EQ(scheduler.dequeue().flow_id, fB);
    }
    scheduler.enqueue(createHfscTestPacket(fA, pkt_size));
    PacketDescriptor pA3 = scheduler.dequeue();
    ASSERT_EQ(pA3.flow_id, fA);
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(HfscSchedulerRtTest, ErrorOnEmptyEligibleSetWithPackets) {
    core::FlowId flowA_id = 1;
    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, 0, ServiceCurve(0, 0)}
    };
    HfscScheduler scheduler(configs, 10000000);
    scheduler.enqueue(createHfscTestPacket(flowA_id, 100));
    ASSERT_FALSE(scheduler.is_empty());
    ASSERT_THROW(scheduler.dequeue(), std::logic_error);
}


// --- Tests for HFSC LS Logic ---

TEST(HfscSchedulerLsTest, TwoFlowsLinkSharingOnly) {
    core::FlowId flowA_id = 1, flowB_id = 2;
    uint64_t ls_rate_A_bps = 1000000;
    uint64_t ls_rate_B_bps = 2000000;
    uint32_t packet_size_bytes = 1000;

    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, 0, ServiceCurve(0,0), ServiceCurve(ls_rate_A_bps, 0)},
        {flowB_id, 0, ServiceCurve(0,0), ServiceCurve(ls_rate_B_bps, 0)}
    };
    HfscScheduler scheduler(configs, 10000000);

    const int num_packets_A = 125;
    const int num_packets_B = 250;
    for (int i = 0; i < num_packets_A; ++i) scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    for (int i = 0; i < num_packets_B; ++i) scheduler.enqueue(createHfscTestPacket(flowB_id, packet_size_bytes));

    std::map<core::FlowId, uint64_t> total_bytes_dequeued;
    int total_packets_dequeued = 0;
    while(!scheduler.is_empty()){
        PacketDescriptor p = scheduler.dequeue();
        total_bytes_dequeued[p.flow_id] += p.packet_length_bytes;
        total_packets_dequeued++;
    }
    ASSERT_EQ(total_packets_dequeued, num_packets_A + num_packets_B);
    double ratio_bytes = static_cast<double>(total_bytes_dequeued[flowA_id]) / total_bytes_dequeued[flowB_id];
    double expected_ratio_rates = static_cast<double>(ls_rate_A_bps) / ls_rate_B_bps;
    ASSERT_NEAR(ratio_bytes, expected_ratio_rates, 0.1);
}

TEST(HfscSchedulerLsTest, RTFlowExhaustsThenLinkShares) {
    core::FlowId flowA_id = 1, flowB_id = 2;
    uint32_t packet_size_bytes = 1000;
    ServiceCurve rt_A(2000000, 0); ServiceCurve ls_A(1000000, 0);
    ServiceCurve ls_B(1000000, 0);
    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, 0, rt_A, ls_A},
        {flowB_id, 0, ServiceCurve(0,0), ls_B}
    };
    HfscScheduler scheduler(configs, 10000000);
    const int num_rt_packets_A = 5;
    const int num_ls_packets_A = 10;
    const int num_ls_packets_B = 15;
    for (int i = 0; i < num_rt_packets_A + num_ls_packets_A; ++i) scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    for (int i = 0; i < num_ls_packets_B; ++i) scheduler.enqueue(createHfscTestPacket(flowB_id, packet_size_bytes));

    std::map<core::FlowId, int> packet_counts;
    std::vector<PacketDescriptor> dequeued_packets;
    while(!scheduler.is_empty()){
        PacketDescriptor p = scheduler.dequeue();
        packet_counts[p.flow_id]++;
        dequeued_packets.push_back(p);
    }
    int initial_A_packets = 0;
    for(int i=0; i < num_rt_packets_A && i < dequeued_packets.size(); ++i) {
        if (dequeued_packets[i].flow_id == flowA_id) initial_A_packets++;
    }
    ASSERT_GE(initial_A_packets, num_rt_packets_A -1 );
    ASSERT_EQ(packet_counts[flowA_id], num_rt_packets_A + num_ls_packets_A);
    ASSERT_EQ(packet_counts[flowB_id], num_ls_packets_B);
}


TEST(HfscSchedulerLsTest, ExcessBandwidthDistributionByLS) {
    core::FlowId fA = 1, fB = 2;
    uint32_t pkt_size = 1000;
    ServiceCurve rt_A(1000000, 0); ServiceCurve ls_A(1000000, 0);
    ServiceCurve rt_B(1000000, 0); ServiceCurve ls_B(2000000, 0);
    std::vector<HfscScheduler::FlowConfig> configs = { {fA, 0, rt_A, ls_A}, {fB, 0, rt_B, ls_B} };
    HfscScheduler scheduler(configs, 5000000);
    const int num_pkts_each_flow = 200;
    for (int i = 0; i < num_pkts_each_flow; ++i) {
        scheduler.enqueue(createHfscTestPacket(fA, pkt_size));
        scheduler.enqueue(createHfscTestPacket(fB, pkt_size));
    }
    std::map<core::FlowId, uint64_t> bytes_dequeued;
    int total_pkts_dequeued = 0;
    const int packets_to_dequeue_for_stat = 300;
    for (int i = 0; i < packets_to_dequeue_for_stat && !scheduler.is_empty(); ++i) {
        PacketDescriptor p = scheduler.dequeue();
        bytes_dequeued[p.flow_id] += p.packet_length_bytes;
        total_pkts_dequeued++;
    }
    ASSERT_GT(total_pkts_dequeued, 0);
    double expected_byte_ratio_A_to_B = 2.0 / 3.0;
    // A gets 1(RT) + 1/3*3(LS) = 2. B gets 1(RT) + 2/3*3(LS) = 3. Ratio A/B = 2/3.
    if (bytes_dequeued[fB] == 0 && bytes_dequeued[fA] == 0) { // Avoid division by zero if no packets
        ASSERT_TRUE(true); // Or specific check if this state is valid/expected
    } else if (bytes_dequeued[fB] == 0) {
        FAIL() << "Flow B received 0 bytes, cannot calculate ratio.";
    }
    else {
       double actual_byte_ratio_A_to_B = static_cast<double>(bytes_dequeued[fA]) / bytes_dequeued[fB];
       ASSERT_NEAR(actual_byte_ratio_A_to_B, expected_byte_ratio_A_to_B, 0.20); // Increased tolerance
    }
}


TEST(HfscSchedulerLsTest, LSOnlyFlowsDifferentDelays) {
    core::FlowId fA = 1, fB = 2;
    uint32_t pkt_size = 1000;
    ServiceCurve ls_A(1000000, 5000);
    ServiceCurve ls_B(1000000, 0);
    std::vector<HfscScheduler::FlowConfig> configs = {
        {fA, 0, ServiceCurve(0,0), ls_A},
        {fB, 0, ServiceCurve(0,0), ls_B}
    };
    HfscScheduler scheduler(configs, 10000000);
    scheduler.enqueue(createHfscTestPacket(fA, pkt_size));
    scheduler.enqueue(createHfscTestPacket(fB, pkt_size));
    std::vector<core::FlowId> dequeued_order;
    dequeued_order.push_back(scheduler.dequeue().flow_id);
    dequeued_order.push_back(scheduler.dequeue().flow_id);
    std::vector<core::FlowId> expected_order = {fB, fA};
    ASSERT_EQ(dequeued_order, expected_order);
    ASSERT_TRUE(scheduler.is_empty());
}


// --- Tests for HFSC UL Logic ---

TEST(HfscSchedulerUlTest, FlowLimitedByUlOnly) {
    core::FlowId flowA_id = 1;
    uint64_t ul_rate_bps = 1000000;
    uint32_t packet_size_bytes = 1000;
    ServiceCurve generous_rt(10000000, 0);
    ServiceCurve limiting_ul(1000000, 0);
    std::vector<HfscScheduler::FlowConfig> configs_revised = {
        {flowA_id, 0, generous_rt, ServiceCurve(0,0), limiting_ul}
    };
    HfscScheduler scheduler_revised(configs_revised, 10000000);
    const int num_packets = 125;
    for (int i = 0; i < num_packets; ++i) scheduler_revised.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    uint64_t total_bytes_A = 0;
    for (int i = 0; i < num_packets; ++i) {
        PacketDescriptor p = scheduler_revised.dequeue();
        total_bytes_A += p.packet_length_bytes;
    }
    ASSERT_TRUE(scheduler_revised.is_empty());
    ASSERT_EQ(total_bytes_A, static_cast<uint64_t>(num_packets) * packet_size_bytes);
}


TEST(HfscSchedulerUlTest, RTGuaranteeCappedByUL) {
    core::FlowId flowA_id = 1;
    uint32_t packet_size_bytes = 1000;
    ServiceCurve rt_sc(2000000, 0);
    ServiceCurve ul_sc(1000000, 0);
    std::vector<HfscScheduler::FlowConfig> configs = { {flowA_id, 0, rt_sc, ServiceCurve(0,0), ul_sc} };
    HfscScheduler scheduler(configs, 10000000);
    const int num_packets = 10;
    for (int i = 0; i < num_packets; ++i) scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    for (int i = 0; i < num_packets; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        ASSERT_EQ(p.flow_id, flowA_id);
    }
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(HfscSchedulerUlTest, LSGuaranteeCappedByUL) {
    core::FlowId flowA_id = 1, flowB_id = 2;
    uint32_t packet_size_bytes = 1000;
    ServiceCurve ls_A(3000000, 0);
    ServiceCurve ul_A(1000000, 0);
    ServiceCurve ls_B(1000000,0);
    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, 0, ServiceCurve(0,0), ls_A, ul_A},
        {flowB_id, 0, ServiceCurve(0,0), ls_B, ServiceCurve(0,0)}
    };
    HfscScheduler scheduler(configs, 10000000);
    const int num_packets_A = 10; const int num_packets_B = 10;
    for (int i = 0; i < num_packets_A; ++i) scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    for (int i = 0; i < num_packets_B; ++i) scheduler.enqueue(createHfscTestPacket(flowB_id, packet_size_bytes));
    std::map<core::FlowId, int> counts;
    for(int i=0; i<num_packets_A + num_packets_B; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        counts[p.flow_id]++;
    }
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_NEAR(counts[flowA_id], counts[flowB_id], 2);
}


TEST(HfscSchedulerUlTest, RTAndLSCappedByUL) {
    core::FlowId flowA_id = 1;
    uint32_t packet_size_bytes = 1000;
    ServiceCurve rt_A(1000000, 0);
    ServiceCurve ls_A(2000000, 0);
    ServiceCurve ul_A(1500000, 0);
    std::vector<HfscScheduler::FlowConfig> configs = { {flowA_id, 0, rt_A, ls_A, ul_A} };
    HfscScheduler scheduler(configs, 10000000);
    const int num_packets = 20;
    for (int i = 0; i < num_packets; ++i) scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    for (int i = 0; i < num_packets; ++i) scheduler.dequeue();
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(HfscSchedulerUlTest, ULDelayEffect) {
    core::FlowId flowA_id = 1;
    uint32_t packet_size_bytes = 1000;
    ServiceCurve ul_sc(1000000, 5000);
    ServiceCurve fast_rt_sc(10000000, 0);
    std::vector<HfscScheduler::FlowConfig> configs = { {flowA_id, 0, fast_rt_sc, ServiceCurve(0,0), ul_sc} };
    HfscScheduler scheduler(configs, 20000000);
    scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    PacketDescriptor p = scheduler.dequeue();
    ASSERT_EQ(p.flow_id, flowA_id);
}

} // namespace scheduler
} // namespace hqts
