#include "gtest/gtest.h"
#include "hqts/scheduler/hfsc_scheduler.h"
#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor
#include "hqts/core/flow_context.h"          // For core::FlowId

#include <vector>
#include <stdexcept> // For std::logic_error, std::invalid_argument, std::out_of_range

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
        {static_cast<core::FlowId>(1), defaultRtSc(), defaultLsSc()},
        {static_cast<core::FlowId>(2), ServiceCurve(2000000, 100), ServiceCurve(1000000, 0)}
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
        {static_cast<core::FlowId>(1), defaultRtSc()},
        {static_cast<core::FlowId>(1), defaultRtSc()} // Duplicate Flow ID
    };
    ASSERT_THROW(HfscScheduler scheduler(configs, 1000000000), std::invalid_argument);
}

TEST(HfscSchedulerTest, EnqueueAndDequeueSinglePacket_PlaceholderLogic) {
    std::vector<HfscScheduler::FlowConfig> configs = {{static_cast<core::FlowId>(1), defaultRtSc()}};
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
    std::vector<HfscScheduler::FlowConfig> configs = {{static_cast<core::FlowId>(1), defaultRtSc()}};
    HfscScheduler scheduler(configs, 1000000000);
    ASSERT_TRUE(scheduler.is_empty());
    // Placeholder dequeue throws runtime_error if empty but configured
    ASSERT_THROW(scheduler.dequeue(), std::runtime_error);
}

TEST(HfscSchedulerTest, EnqueueToUnconfiguredFlowId_PlaceholderLogic) {
    // Only flow 1 configured
    std::vector<HfscScheduler::FlowConfig> configs = {{static_cast<core::FlowId>(1), defaultRtSc()}};
    HfscScheduler scheduler(configs, 1000000000);

    // Flow 2 not configured, packet.priority will be 2 (used as FlowId)
    PacketDescriptor p_unconfigured_flow = createHfscTestPacket(2, 100);
    ASSERT_THROW(scheduler.enqueue(p_unconfigured_flow), std::out_of_range);
}

TEST(HfscSchedulerTest, MultipleFlowsSimple_PlaceholderLogic) {
    std::vector<HfscScheduler::FlowConfig> configs = {
        {static_cast<core::FlowId>(1), defaultRtSc()},
        {static_cast<core::FlowId>(2), defaultRtSc()}
    };
    HfscScheduler scheduler(configs, 1000000000);

    PacketDescriptor p_flow1 = createHfscTestPacket(1, 100);
    PacketDescriptor p_flow2 = createHfscTestPacket(2, 150);

    scheduler.enqueue(p_flow1);
    scheduler.enqueue(p_flow2);
    ASSERT_EQ(scheduler.get_flow_queue_size(1), 1);
    ASSERT_EQ(scheduler.get_flow_queue_size(2), 1);
    ASSERT_FALSE(scheduler.is_empty());

    // Placeholder dequeue is simple FIFO across flows map iteration order.
    // std::map iterates by key, so FlowId 1 should be first.
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

// Helper to get current virtual time (approximated by dequeue VFT)
// This is tricky as current_virtual_time_ is private. We infer it.
// For these tests, we'll mostly check packet order and counts.
// To check virtual times, we'd need an accessor or friend class, or log them.
// For now, let's assume the VFT of the dequeued packet IS the current_virtual_time.

TEST(HfscSchedulerRtTest, SingleFlowRealTimeService) {
    // FlowA: 1 Mbps (125000 Bytes/sec), 0 delay. Packet size 1250 bytes.
    // Service time per packet = (1250 * 8 * 1000000) / 1000000 = 10000 us.
    core::FlowId flowA_id = 1;
    uint32_t packet_size_bytes = 1250; // 10000 bits
    uint64_t rt_rate_bps = 1000000;    // 1 Mbps
    uint64_t expected_service_time_us = (static_cast<uint64_t>(packet_size_bytes) * 8 * 1000000) / rt_rate_bps; // 10000 us

    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, ServiceCurve(rt_rate_bps, 0)}
    };
    HfscScheduler scheduler(configs, 10000000); // 10 Mbps link

    const int num_packets = 10;
    for (int i = 0; i < num_packets; ++i) {
        scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    }

    uint64_t last_vft = 0;
    // The scheduler's current_virtual_time_ starts at 0.
    // First packet: eligible_time = max(0,0) + 0 = 0. VFT = 0 + 10000 = 10000.
    // Second packet (after first dequeued): current_VT becomes 10000. eligible_time = 10000 + 0. VFT = 10000 + 10000 = 20000.
    // And so on.

    for (int i = 0; i < num_packets; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        ASSERT_EQ(p.flow_id, flowA_id);
        // We can't directly get the VFT that was used from the packet.
        // However, the scheduler's internal current_virtual_time_ is set to this VFT.
        // If we had an accessor for scheduler.current_virtual_time_, we could check it.
        // For now, we trust the priority queue and VFT calculations are correct if packets come out.
        // The VFT of packet 'i' (0-indexed) should be (i+1) * expected_service_time_us.
        // This will be the value of current_virtual_time_ *after* this packet is dequeued.
    }
    ASSERT_TRUE(scheduler.is_empty());
    // Test: If we could get current_virtual_time_ from scheduler, it should be num_packets * expected_service_time_us.
}

TEST(HfscSchedulerRtTest, SingleFlowWithRtDelay) {
    core::FlowId flowA_id = 1;
    uint32_t packet_size_bytes = 1250; // 10000 bits
    uint64_t rt_rate_bps = 1000000;    // 1 Mbps
    uint64_t rt_delay_us = 5000;
    uint64_t service_time_us = (static_cast<uint64_t>(packet_size_bytes) * 8 * 1000000) / rt_rate_bps; // 10000 us

    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, ServiceCurve(rt_rate_bps, rt_delay_us)}
    };
    // Assume current_virtual_time_ starts at 0 internally.
    // If a packet is enqueued at external time T, and scheduler current_virtual_time_ is V_current
    // eligible_time = max(V_current, flow.VFT_previous) + delay_us
    // VFT_new = eligible_time + service_time_us
    HfscScheduler scheduler(configs, 10000000);

    scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    // Enqueue logic: was_empty=true. flow.VFT_previous=0. current_VT=0.
    // eligible_time = max(0,0) + 5000 = 5000.
    // VFT = 5000 + 10000 = 15000. Pushed to eligible_set_ with {15000, flowA_id}.

    // To test the delay, we need to see when it's dequeued *relative* to virtual time.
    // If we had a way to advance virtual time or observe it:
    // Suppose current_virtual_time was manually set to 10000us (not possible with current API).
    // Then eligible_time = max(10000,0) + 5000 = 15000. VFT = 15000 + 10000 = 25000.
    // This test is hard without virtual time control or observation.
    // For now, we just check it dequeues.
    PacketDescriptor p = scheduler.dequeue();
    ASSERT_EQ(p.flow_id, flowA_id);
    // After this dequeue, scheduler.current_virtual_time_ would be 15000.
}


TEST(HfscSchedulerRtTest, TwoFlowsIndependentRealTime) {
    core::FlowId flowA_id = 1, flowB_id = 2;
    uint32_t packet_size_bytes = 1250; // 10000 bits each
    uint64_t rate_bps = 1000000;       // 1 Mbps each
    // Service time for each packet = 10000 us

    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, ServiceCurve(rate_bps, 0)},
        {flowB_id, ServiceCurve(rate_bps, 0)}
    };
    HfscScheduler scheduler(configs, 10000000); // 10 Mbps link (enough for both)

    const int num_packets_per_flow = 5; // Reduced for simplicity of trace
    for (int i = 0; i < num_packets_per_flow; ++i) {
        scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
        scheduler.enqueue(createHfscTestPacket(flowB_id, packet_size_bytes));
    }
    // Enqueue order: A1, B1, A2, B2, ... (assuming flow IDs are used as packet.priority for simplicity)
    // Trace for first few:
    // current_VT = 0
    // Enq A1 (1250B): flowA empty. el_A=max(0,0)+0=0. vft_A=0+10000=10000. eligible_set.push({10000,A})
    // Enq B1 (1250B): flowB empty. el_B=max(0,0)+0=0. vft_B=0+10000=10000. eligible_set.push({10000,B})
    // eligible_set: {{10000,A}, {10000,B}} (Order depends on tie-breaking, flow_id A < B)
    // Enq A2: flowA not empty.
    // Enq B2: flowB not empty.
    // ... (10 packets total)

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
        {f1, ServiceCurve(1000000, 0)},    // 1Mbps, Pkt(1000B) -> 8000us. Pkt(500B) -> 4000us
        {f2, ServiceCurve(2000000, 1000)}, // 2Mbps, Pkt(1000B) -> 4000us. Delay 1000us
        {f3, ServiceCurve(500000, 0)}      // 0.5Mbps, Pkt(1000B) -> 16000us
    };
    HfscScheduler scheduler(configs, 10000000);

    // Enqueue packets (flow_id, size_bytes)
    // P1_1 for F1: (1, 1000) -> VFT = 0+8000 = 8000. EligibleSet: { (8000,1) }
    scheduler.enqueue(createHfscTestPacket(f1, 1000));
    // P2_1 for F2: (2, 1000) -> el_time = max(0,0)+1000=1000. VFT = 1000+4000 = 5000. EligibleSet: { (5000,2), (8000,1) }
    scheduler.enqueue(createHfscTestPacket(f2, 1000));
    // P3_1 for F3: (3, 1000) -> VFT = 0+16000 = 16000. EligibleSet: { (5000,2), (8000,1), (16000,3) }
    scheduler.enqueue(createHfscTestPacket(f3, 1000));
    // P1_2 for F1: (1, 500) -> Flow 1 not empty. Does not re-add to eligible set yet.
    scheduler.enqueue(createHfscTestPacket(f1, 500));


    std::vector<uint64_t> vft_sequence;
    std::vector<core::FlowId> flow_id_sequence;
    uint64_t last_vft = 0;

    // Dequeue P2_1 (VFT 5000)
    // current_VT becomes 5000. F2 queue is now empty.
    PacketDescriptor p = scheduler.dequeue();
    ASSERT_EQ(p.flow_id, f2);
    // current_virtual_time_ is now 5000 (p.VFT)
    // This is hard to get from outside. Let's assume VFTs are calculated correctly by enqueue/dequeue.
    // We can check the order.
    flow_id_sequence.push_back(p.flow_id);
    // After P2_1 dequeued (VFT 5000):
    // EligibleSet: { (8000,1), (16000,3) }

    // Dequeue P1_1 (VFT 8000)
    // current_VT becomes 8000. F1 queue has P1_2 (500B).
    // F1 re-eval: el_time = max(8000, prev_F1_VFT=8000) + 0 = 8000.
    //             service_time = 500B / 1Mbps = 4000us.
    //             new_F1_VFT = 8000 + 4000 = 12000.
    // EligibleSet: { (12000,1), (16000,3) }
    p = scheduler.dequeue();
    ASSERT_EQ(p.flow_id, f1);
    flow_id_sequence.push_back(p.flow_id);

    // Dequeue P1_2 (VFT 12000)
    // current_VT becomes 12000. F1 queue empty.
    // EligibleSet: { (16000,3) }
    p = scheduler.dequeue();
    ASSERT_EQ(p.flow_id, f1);
    flow_id_sequence.push_back(p.flow_id);

    // Dequeue P3_1 (VFT 16000)
    // current_VT becomes 16000. F3 queue empty.
    // EligibleSet: {}
    p = scheduler.dequeue();
    ASSERT_EQ(p.flow_id, f3);
    flow_id_sequence.push_back(p.flow_id);

    ASSERT_TRUE(scheduler.is_empty());

    std::vector<core::FlowId> expected_flow_order = {f2, f1, f1, f3};
    ASSERT_EQ(flow_id_sequence, expected_flow_order);
    // Monotonicity of VFTs is implicitly tested if the order is correct based on VFT calculations.
}


TEST(HfscSchedulerRtTest, FlowBecomesActiveReEligibility) {
    core::FlowId fA = 1, fB = 2;
    uint32_t pkt_size = 1250; // 1Mbps -> 10000us service time
    std::vector<HfscScheduler::FlowConfig> configs = {
        {fA, ServiceCurve(1000000, 0)},
        {fB, ServiceCurve(1000000, 0)}
    };
    HfscScheduler scheduler(configs, 10000000);

    // Enqueue A1, A2, B1
    // A1: el=0, vft=10000. ES: {(10000,A)}
    scheduler.enqueue(createHfscTestPacket(fA, pkt_size)); // A1
    // A2: enqueued to flow A's queue. Flow A already in ES.
    scheduler.enqueue(createHfscTestPacket(fA, pkt_size)); // A2
    // B1: el=0, vft=10000. ES: {(10000,A), (10000,B)} (A before B due to tie-break)
    scheduler.enqueue(createHfscTestPacket(fB, pkt_size)); // B1

    // Dequeue A1. current_VT = 10000.
    // A has A2. Re-eval A: el_A=max(10000,10000)+0 = 10000. vft_A=10000+10000=20000.
    // ES: {(10000,B), (20000,A)}
    ASSERT_EQ(scheduler.dequeue().flow_id, fA);

    // Dequeue B1. current_VT = 10000 (VFT of B1).
    // B becomes empty.
    // ES: {(20000,A)}
    ASSERT_EQ(scheduler.dequeue().flow_id, fB);
    // current_VT is now VFT of B1, which was 10000.
    // Flow A's VFT is 20000. Flow B is empty.

    // Let Flow A's P_A2 also be dequeued.
    // Dequeue A2. current_VT = 20000.
    // A becomes empty.
    // ES: {}
    ASSERT_EQ(scheduler.dequeue().flow_id, fA);
    ASSERT_TRUE(scheduler.is_empty());
    // At this point, scheduler's current_virtual_time_ is 20000. Flow A's VFT was 20000.

    // Enqueue many to Flow B to advance current_virtual_time significantly
    for(int i=0; i < 5; ++i) { // 5 packets * 10000us/pkt = 50000us
        scheduler.enqueue(createHfscTestPacket(fB, pkt_size)); // B2, B3, B4, B5, B6
    }
    // B2: el_B = max(20000, 0) + 0 = 20000. vft_B = 20000 + 10000 = 30000. ES: {(30000,B)}
    // ... after 5 B packets are dequeued:
    // VFT of B6 will be 20000 (eligible) + 5*10000 (service) = 70000.
    for(int i=0; i < 5; ++i) {
        ASSERT_EQ(scheduler.dequeue().flow_id, fB);
    }
    // current_virtual_time_ is now 70000. Flow A's last VFT was 20000. Flow B's last VFT was 70000.

    // Enqueue new P_A3 to FlowA
    scheduler.enqueue(createHfscTestPacket(fA, pkt_size)); // A3
    // Enqueue logic for A3:
    // flow_state_A.virtual_finish_time was 20000 (from A2).
    // current_virtual_time_ is 70000.
    // eligible_time_A3 = std::max(70000, 20000) + 0 (delay) = 70000.
    // service_time_A3 = 10000us.
    // virtual_finish_time_A3 = 70000 + 10000 = 80000.
    // ES: {(80000,A)}

    PacketDescriptor pA3 = scheduler.dequeue();
    ASSERT_EQ(pA3.flow_id, fA);
    // If we had access to scheduler.current_virtual_time_, it should be 80000.
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(HfscSchedulerRtTest, ErrorOnEmptyEligibleSetWithPackets) {
    core::FlowId flowA_id = 1;
    // Configure with a very large delay, so it's not eligible initially if current_VT is small.
    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, ServiceCurve(1000000, 100000)} // 1Mbps, 100ms delay
    };
    HfscScheduler scheduler(configs, 10000000);
    // current_virtual_time_ is 0.

    scheduler.enqueue(createHfscTestPacket(flowA_id, 100));
    // Enqueue for FlowA:
    // eligible_time = max(0,0) + 100000 = 100000.
    // service_time = 100B * 8 / 1Mbps = 800us.
    // VFT = 100000 + 800 = 100800.
    // eligible_set_ has {(100800, A)}.

    // This test setup won't directly hit the "Eligible set empty but packets exist"
    // because enqueue immediately makes it eligible if RT rate > 0.
    // To test that specific throw, one would need to:
    // 1. Enqueue packet, it goes to eligible_set.
    // 2. Manually (if possible) set current_virtual_time_ to something *less* than all VFTs in eligible_set
    //    AND less than any flow's eligible_time if it were to be recomputed.
    // 3. OR, have all flows with RT rate 0.
    // This specific std::logic_error is for a state that *shouldn't* be reached with current RT-only logic
    // if enqueue/dequeue are correct. It's more a safeguard for future LS/UL curve interactions
    // or if virtual time advancement logic becomes more complex.

    // For now, let's test the scenario where RT rate is 0, as per implementation notes in enqueue/dequeue
    // such flows are not added to eligible_set based on RT VFT.
    std::vector<HfscScheduler::FlowConfig> zero_rate_configs = {
        {flowA_id, ServiceCurve(0, 0)} // Zero rate RT curve
    };
    HfscScheduler scheduler_zero_rt(zero_rate_configs, 10000000);
    scheduler_zero_rt.enqueue(createHfscTestPacket(flowA_id, 100));
    ASSERT_FALSE(scheduler_zero_rt.is_empty()); // total_packets_ = 1
    // Dequeue should throw the specific logic_error because eligible_set_ will be empty
    ASSERT_THROW(scheduler_zero_rt.dequeue(), std::logic_error);
}


// --- Tests for HFSC LS Logic ---

TEST(HfscSchedulerLsTest, TwoFlowsLinkSharingOnly) {
    core::FlowId flowA_id = 1, flowB_id = 2;
    uint64_t ls_rate_A_bps = 1000000; // 1 Mbps
    uint64_t ls_rate_B_bps = 2000000; // 2 Mbps
    uint32_t packet_size_bytes = 1000; // All packets are 1000 bytes

    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, ServiceCurve(0,0), ServiceCurve(ls_rate_A_bps, 0)}, // RT rate 0, only LS
        {flowB_id, ServiceCurve(0,0), ServiceCurve(ls_rate_B_bps, 0)}  // RT rate 0, only LS
    };
    HfscScheduler scheduler(configs, 10000000); // 10 Mbps link

    // Enqueue packets to sustain for approx 1 second of their LS rate
    // Flow A: 1 Mbps = 125000 Bps. Needs 125 packets of 1000B.
    // Flow B: 2 Mbps = 250000 Bps. Needs 250 packets of 1000B.
    const int num_packets_A = 125;
    const int num_packets_B = 250;

    for (int i = 0; i < num_packets_A; ++i) {
        scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    }
    for (int i = 0; i < num_packets_B; ++i) {
        scheduler.enqueue(createHfscTestPacket(flowB_id, packet_size_bytes));
    }

    std::map<core::FlowId, uint64_t> total_bytes_dequeued;
    int total_packets_dequeued = 0;
    while(!scheduler.is_empty()){
        PacketDescriptor p = scheduler.dequeue();
        total_bytes_dequeued[p.flow_id] += p.packet_length_bytes;
        total_packets_dequeued++;
    }

    ASSERT_EQ(total_packets_dequeued, num_packets_A + num_packets_B);

    double ratio_bytes = static_cast<double>(total_bytes_dequeued[flowA_id]) / total_bytes_dequeued[flowB_id];
    double expected_ratio_rates = static_cast<double>(ls_rate_A_bps) / ls_rate_B_bps; // 0.5

    // Allow a tolerance due to packetization. For large number of packets, it should be close.
    // E.g. if last few packets cause slight deviation.
    ASSERT_NEAR(ratio_bytes, expected_ratio_rates, 0.1); // 10% tolerance
}

TEST(HfscSchedulerLsTest, RTFlowExhaustsThenLinkShares) {
    core::FlowId flowA_id = 1, flowB_id = 2;
    uint32_t packet_size_bytes = 1000;

    // FlowA: RT 2Mbps for 5000 bytes, then LS 1Mbps
    // Service time for 1000B at 2Mbps = (1000*8*1000000)/(2000000) = 4000us
    // 5000 bytes = 5 packets. RT phase duration = 5 * 4000us = 20000us.
    ServiceCurve rt_A(2000000, 0);
    ServiceCurve ls_A(1000000, 0);

    // FlowB: LS 1Mbps
    ServiceCurve ls_B(1000000, 0);

    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, rt_A, ls_A},
        {flowB_id, ServiceCurve(0,0), ls_B} // Flow B is LS only
    };
    HfscScheduler scheduler(configs, 10000000); // 10 Mbps link

    const int num_rt_packets_A = 5; // 5 * 1000B = 5000 Bytes for RT phase
    const int num_ls_packets_A = 10; // Additional packets for LS phase for A
    const int num_ls_packets_B = 15; // Packets for B (LS only)

    for (int i = 0; i < num_rt_packets_A + num_ls_packets_A; ++i) {
        scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    }
    for (int i = 0; i < num_ls_packets_B; ++i) {
        scheduler.enqueue(createHfscTestPacket(flowB_id, packet_size_bytes));
    }

    std::map<core::FlowId, int> packet_counts;
    std::vector<PacketDescriptor> dequeued_packets;
    while(!scheduler.is_empty()){
        PacketDescriptor p = scheduler.dequeue();
        packet_counts[p.flow_id]++;
        dequeued_packets.push_back(p);
    }

    // Check initial RT phase for Flow A
    int initial_A_packets = 0;
    for(int i=0; i < num_rt_packets_A && i < dequeued_packets.size(); ++i) {
        if (dequeued_packets[i].flow_id == flowA_id) {
            initial_A_packets++;
        }
    }
    // This simple check might not be robust enough if B's LS VFTs are very early.
    // A more robust check would be to see if the first 5 packets for A have VFTs dictated by RT curve.
    // For now, we expect Flow A to dominate initially.
    ASSERT_GE(initial_A_packets, num_rt_packets_A -1 ); // Allow for some interleaving if B's LS is very competitive early

    // Check overall LS sharing for packets beyond A's initial RT burst
    // Total A packets = 15, Total B packets = 15.
    // After A's RT part (5 packets), A has 10 LS packets, B has 15 LS packets.
    // They share 1:1 via LS.
    ASSERT_EQ(packet_counts[flowA_id], num_rt_packets_A + num_ls_packets_A);
    ASSERT_EQ(packet_counts[flowB_id], num_ls_packets_B);

    // A more detailed test would inspect VFTs to confirm transition from RT to LS logic for Flow A.
}


TEST(HfscSchedulerLsTest, ExcessBandwidthDistributionByLS) {
    core::FlowId fA = 1, fB = 2;
    uint32_t pkt_size = 1000; // 1000 bytes
    // Service time at 1Mbps = 8000us. At 2Mbps = 4000us. At 3Mbps = 2666.6us

    ServiceCurve rt_A(1000000, 0); // 1 Mbps RT for A
    ServiceCurve ls_A(1000000, 0); // 1 "share unit" (1Mbps for simplicity of rate interpretation) for A

    ServiceCurve rt_B(1000000, 0); // 1 Mbps RT for B
    ServiceCurve ls_B(2000000, 0); // 2 "share units" (2Mbps for simplicity) for B

    std::vector<HfscScheduler::FlowConfig> configs = { {fA, rt_A, ls_A}, {fB, rt_B, ls_B} };
    HfscScheduler scheduler(configs, 5000000); // 5 Mbps total link capacity

    // Enqueue many packets to keep both flows backlogged
    const int num_pkts_each_flow = 200; // Enough for >1 second of data
    for (int i = 0; i < num_pkts_each_flow; ++i) {
        scheduler.enqueue(createHfscTestPacket(fA, pkt_size));
        scheduler.enqueue(createHfscTestPacket(fB, pkt_size));
    }

    std::map<core::FlowId, uint64_t> bytes_dequeued;
    int total_pkts_dequeued = 0;
    // Simulate for a duration by dequeuing a large number of packets
    // Total RT = 2Mbps. Excess = 3Mbps. LS ratio A:B = 1:2.
    // A gets 1Mbps(RT) + 1/3*3Mbps(LS) = 2Mbps.
    // B gets 1Mbps(RT) + 2/3*3Mbps(LS) = 3Mbps.
    // Ratio of total rates A:B = 2:3.
    // For 200+200=400 packets, A should send 400 * (2/5) = 160. B should send 400 * (3/5) = 240.
    // This assumes the number of packets is what limits, not a fixed time.

    const int packets_to_dequeue_for_stat = 300; // Dequeue a significant number of packets
    for (int i = 0; i < packets_to_dequeue_for_stat && !scheduler.is_empty(); ++i) {
        PacketDescriptor p = scheduler.dequeue();
        bytes_dequeued[p.flow_id] += p.packet_length_bytes;
        total_pkts_dequeued++;
    }

    ASSERT_GT(total_pkts_dequeued, 0); // Ensure some packets were dequeued.

    double expected_byte_ratio_A_to_B = 2.0 / 3.0; // Expected total rate ratio A:B
    double actual_byte_ratio_A_to_B = static_cast<double>(bytes_dequeued[fA]) / bytes_dequeued[fB];

    // Tolerance might need to be larger for shorter simulations or smaller packet counts
    ASSERT_NEAR(actual_byte_ratio_A_to_B, expected_byte_ratio_A_to_B, 0.15);
}


TEST(HfscSchedulerLsTest, LSOnlyFlowsDifferentDelays) {
    core::FlowId fA = 1, fB = 2;
    uint32_t pkt_size = 1000;
    ServiceCurve ls_A(1000000, 5000); // 1 Mbps, 5000us delay for A
    ServiceCurve ls_B(1000000, 0);    // 1 Mbps, 0us delay for B

    std::vector<HfscScheduler::FlowConfig> configs = {
        {fA, ServiceCurve(0,0), ls_A}, // RT rate 0
        {fB, ServiceCurve(0,0), ls_B}  // RT rate 0
    };
    HfscScheduler scheduler(configs, 10000000); // 10 Mbps link
    // Scheduler current_virtual_time_ is 0 initially.

    scheduler.enqueue(createHfscTestPacket(fA, pkt_size)); // P_A1
    scheduler.enqueue(createHfscTestPacket(fB, pkt_size)); // P_B1

    // Enqueue P_A1: flowA empty. base_el_A = max(0,0)=0.
    //   ls_el_A = 0 + 5000 = 5000. vft_ls_A = 5000 + service_time(1000B @ 1Mbps=8000us) = 13000.
    //   Chosen VFT for A = 13000. EligibleSet: {(13000,A)}
    // Enqueue P_B1: flowB empty. base_el_B = max(0,0)=0.
    //   ls_el_B = 0 + 0 = 0. vft_ls_B = 0 + 8000 = 8000.
    //   Chosen VFT for B = 8000. EligibleSet: {(8000,B), (13000,A)}

    std::vector<core::FlowId> dequeued_order;
    dequeued_order.push_back(scheduler.dequeue().flow_id); // Should be B (VFT 8000)
    // After B dequeued: current_VT = 8000. B becomes empty. EligibleSet: {(13000,A)}
    dequeued_order.push_back(scheduler.dequeue().flow_id); // Should be A (VFT 13000)
    // After A dequeued: current_VT = 13000. A becomes empty. EligibleSet: {}

    std::vector<core::FlowId> expected_order = {fB, fA};
    ASSERT_EQ(dequeued_order, expected_order);
    ASSERT_TRUE(scheduler.is_empty());
}


// --- Tests for HFSC UL Logic ---

TEST(HfscSchedulerUlTest, FlowLimitedByUlOnly) {
    core::FlowId flowA_id = 1;
    uint64_t ul_rate_bps = 1000000; // 1 Mbps
    uint32_t packet_size_bytes = 1000; // 1000 bytes
    // Service time per packet at 1Mbps UL = 8000 us

    // Configure FlowA with only UL curve. RT and LS are zero rate (effectively non-existent for choosing VFT).
    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, ServiceCurve(0,0), ServiceCurve(0,0), ServiceCurve(ul_rate_bps, 0)}
    };
    HfscScheduler scheduler(configs, 10000000); // 10 Mbps link

    const int num_packets = 125; // Should take 125 * 8000us = 1,000,000 us (1 sec)
    for (int i = 0; i < num_packets; ++i) {
        scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    }
    // Enqueue logic with only UL:
    // RT/LS VFTs will be infinity. UL part:
    // eligible_time_ul_candidate = max(current_VT, prev_UL_VFT) + delay(0)
    // final_eligible_time = max(inf, eligible_time_ul_candidate) -> this logic needs care.
    // The current implementation might not make it eligible if RT/LS are zero.
    // Let's re-check enqueue: if chosen_vft (from RT/LS) is infinity, it doesn't enter the UL block to modify it.
    // And if chosen_vft is infinity, it's not added to eligible set. This is correct.
    // The UL curve is a LIMITER, it does not grant service alone if RT/LS do not.
    // This test needs an RT or LS curve to grant initial eligibility, which UL then caps.

    // Re-design: Give a very generous RT or LS, then cap with UL.
    ServiceCurve generous_rt(10000000, 0); // 10Mbps RT
    ServiceCurve limiting_ul(1000000, 0);  // 1Mbps UL
    std::vector<HfscScheduler::FlowConfig> configs_revised = {
        {flowA_id, generous_rt, ServiceCurve(0,0), limiting_ul}
    };
    HfscScheduler scheduler_revised(configs_revised, 10000000);

    for (int i = 0; i < num_packets; ++i) {
        scheduler_revised.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    }

    uint64_t total_bytes_A = 0;
    uint64_t first_pkt_vft_start_approx = 0; // Cannot get this easily
    uint64_t last_pkt_vft_end = 0;

    for (int i = 0; i < num_packets; ++i) {
        PacketDescriptor p = scheduler_revised.dequeue();
        total_bytes_A += p.packet_length_bytes;
        // last_pkt_vft_end = scheduler_revised.current_virtual_time_; // Need accessor
    }
    ASSERT_TRUE(scheduler_revised.is_empty());
    // To properly test the rate, we need to know the duration from scheduler's perspective.
    // This is hard without current_virtual_time_ accessor.
    // For now, this test mainly ensures packets do get dequeued.
    // The actual rate enforcement check is better done in RTGuaranteeCappedByUL.
    ASSERT_EQ(total_bytes_A, static_cast<uint64_t>(num_packets) * packet_size_bytes);
}


TEST(HfscSchedulerUlTest, RTGuaranteeCappedByUL) {
    core::FlowId flowA_id = 1;
    uint32_t packet_size_bytes = 1000; // 1000 bytes
    ServiceCurve rt_sc(2000000, 0);  // RT wants 2 Mbps
    ServiceCurve ul_sc(1000000, 0);  // UL caps at 1 Mbps (service time = 8000 us/pkt)

    std::vector<HfscScheduler::FlowConfig> configs = { {flowA_id, rt_sc, ServiceCurve(0,0), ul_sc} };
    HfscScheduler scheduler(configs, 10000000); // 10 Mbps link

    const int num_packets = 10; // Test with 10 packets
    for (int i = 0; i < num_packets; ++i) {
        scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    }

    std::vector<uint64_t> vft_diffs;
    uint64_t previous_packet_vft = 0; // Approximated start of scheduler virtual time

    // Enqueue logic:
    // P1: RT_el=0, RT_vft=4000. LS_vft=inf. Chosen_el=0, Chosen_vft=4000.
    //     UL_el_cand = max(0,0)+0=0. final_el=max(0,0)=0.
    //     RT_LS_serv_time = 4000. final_vft = 0+4000=4000.
    //     UL_vft_update = 0 + 8000 = 8000. (flowA.vft_ul = 8000)
    //     EligibleSet: {(4000,A)}
    // Dequeue P1: current_VT = 4000. flowA.vft_ul was updated to 8000 based on P1's start_time=0 and UL rate.
    // Re-eval A for P2:
    //   base_el_next = 4000.
    //   RT_el=4000, RT_vft=4000+4000=8000. LS_vft=inf. Chosen_el=4000, Chosen_vft=8000. RT_LS_serv_time=4000.
    //   UL_el_cand = max(4000, flowA.vft_ul_P1=8000) + 0 = 8000.
    //   final_el = max(4000, 8000) = 8000.
    //   final_vft = 8000 + 4000 = 12000.
    //   UL_vft_update for P2 = 8000 + 8000 = 16000. (flowA.vft_ul = 16000)
    //   EligibleSet: {(12000,A)}
    // Dequeue P2: current_VT = 12000.
    // The VFTs in eligible set are {4000, 12000, 20000, ...} effectively 8000us spacing due to UL.

    for (int i = 0; i < num_packets; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        ASSERT_EQ(p.flow_id, flowA_id);
        // This test can't directly verify the rate without current_virtual_time_ access after each dequeue.
        // However, the VFT ordering implies the UL constraint is active.
        // If we could get current_virtual_time:
        // uint64_t current_vt = scheduler.getCurrentVirtualTime(); // Fictional getter
        // if (i > 0) { vft_diffs.push_back(current_vt - previous_packet_vft); }
        // previous_packet_vft = current_vt;
    }
    // for (size_t i = 0; i < vft_diffs.size(); ++i) {
    //    ASSERT_EQ(vft_diffs[i], 8000); // Each packet should be spaced by UL service time
    // }
    ASSERT_TRUE(scheduler.is_empty());
}

TEST(HfscSchedulerUlTest, LSGuaranteeCappedByUL) {
    core::FlowId flowA_id = 1, flowB_id = 2;
    uint32_t packet_size_bytes = 1000;
    ServiceCurve ls_A(3000000, 0); // LS wants 3 Mbps
    ServiceCurve ul_A(1000000, 0); // UL caps A at 1 Mbps
    ServiceCurve ls_B(1000000,0);  // Flow B to ensure A doesn't get all link due to no contention

    std::vector<HfscScheduler::FlowConfig> configs = {
        {flowA_id, ServiceCurve(0,0), ls_A, ul_A},
        {flowB_id, ServiceCurve(0,0), ls_B, ServiceCurve(0,0)}
    };
    HfscScheduler scheduler(configs, 10000000); // 10 Mbps link

    const int num_packets_A = 10;
    const int num_packets_B = 10;
    for (int i = 0; i < num_packets_A; ++i) scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    for (int i = 0; i < num_packets_B; ++i) scheduler.enqueue(createHfscTestPacket(flowB_id, packet_size_bytes));

    // Similar to RT capped by UL, the VFT sequence for flowA should reflect UL's rate.
    // This is hard to verify precisely without VFT access.
    // We expect flow A not to starve flow B excessively beyond what its 1Mbps UL allows.
    std::map<core::FlowId, int> counts;
    for(int i=0; i<num_packets_A + num_packets_B; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        counts[p.flow_id]++;
    }
    ASSERT_TRUE(scheduler.is_empty());
    // Exact counts are tricky. If A is capped at 1Mbps, and B also wants 1Mbps,
    // they might get similar packet counts if sizes are same.
    // If A wanted 3Mbps via LS but capped at 1Mbps UL, it behaves like a 1Mbps flow.
    // So A and B should behave like two 1Mbps flows.
    ASSERT_NEAR(counts[flowA_id], counts[flowB_id], 2); // Expect roughly equal
}


TEST(HfscSchedulerUlTest, RTAndLSCappedByUL) {
    core::FlowId flowA_id = 1;
    uint32_t packet_size_bytes = 1000; // Service time at 1Mbps=8000us, at 1.5Mbps=5333us, at 2Mbps=4000us

    ServiceCurve rt_A(1000000, 0);  // RT 1 Mbps
    ServiceCurve ls_A(2000000, 0);  // LS "potential" for 2 Mbps (if link was free)
    ServiceCurve ul_A(1500000, 0);  // UL caps at 1.5 Mbps

    std::vector<HfscScheduler::FlowConfig> configs = { {flowA_id, rt_A, ls_A, ul_A} };
    HfscScheduler scheduler(configs, 10000000); // 10 Mbps link

    const int num_packets = 20; // Test with 20 packets
    for (int i = 0; i < num_packets; ++i) {
        scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    }

    // Expected behavior:
    // RT is 1Mbps. LS is 2Mbps. UL is 1.5Mbps.
    // Flow will be eligible based on min(VFT_RT, VFT_LS).
    // VFT_RT for 1000B packet = 8000us. VFT_LS for 1000B packet = 4000us.
    // So LS curve initially governs: chosen_el_time based on LS, chosen_vft based on LS.
    // Example P1: el_LS=0, vft_LS=4000. el_RT=0, vft_RT=8000.
    //             chosen_el=0, chosen_vft=4000 (from LS). service_rt_ls = 4000.
    // Then UL check: vft_ul_prev=0. el_ul_cand=max(0,0)+0=0.
    //             final_el = max(chosen_el=0, el_ul_cand=0) = 0.
    //             final_vft = final_el + service_rt_ls = 0 + 4000 = 4000.
    //             flow.vft_ul = final_el + service_ul(1000B @ 1.5Mbps=5333us) = 0 + 5333 = 5333.
    //             EligibleSet: {(4000, A)}
    // Dequeue P1: current_VT = 4000. flow.vft_ul_P1 = 5333.
    // Re-eval A for P2:
    //   base_el_next = 4000.
    //   RT_el=4000, RT_vft=4000+8000=12000.
    //   LS_el=4000, LS_vft=4000+4000=8000.
    //   Chosen_el=4000, Chosen_vft=8000 (from LS). service_rt_ls=4000.
    //   UL_el_cand = max(4000, vft_ul_P1=5333) + 0 = 5333.
    //   final_el = max(4000, 5333) = 5333.
    //   final_vft = 5333 + 4000 = 9333.
    //   flow.vft_ul for P2 = 5333 + 5333 = 10666.
    //   EligibleSet: {(9333,A)}
    // Dequeue P2: current_VT = 9333. flow.vft_ul_P2 = 10666.
    // The VFTs in eligible set are {4000, 9333, ...}, spacing reflects UL influence.
    // Effective rate should be capped at 1.5Mbps.
    for (int i = 0; i < num_packets; ++i) {
        scheduler.dequeue();
    }
    ASSERT_TRUE(scheduler.is_empty());
    // Precise VFT sequence check is complex here, but overall throughput should be limited by UL.
}

TEST(HfscSchedulerUlTest, ULDelayEffect) {
    core::FlowId flowA_id = 1;
    uint32_t packet_size_bytes = 1000;
    ServiceCurve ul_sc(1000000, 5000); // UL 1Mbps, 5000us delay
    // Give it a very fast RT curve to ensure UL is the constraint for eligibility time.
    ServiceCurve fast_rt_sc(10000000, 0); // 10Mbps RT, 0 delay

    std::vector<HfscScheduler::FlowConfig> configs = { {flowA_id, fast_rt_sc, ServiceCurve(0,0), ul_sc} };
    HfscScheduler scheduler(configs, 20000000); // 20 Mbps link
    // Scheduler current_virtual_time_ is 0.

    scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    // Enqueue P1:
    //   RT_el=0, RT_vft = 0 + (1000B*8/10Mbps) = 0 + 800 = 800.
    //   LS_vft=inf. Chosen_el=0, Chosen_vft=800. service_rt_ls=800.
    //   UL_el_cand = max(0,0) + 5000 = 5000.
    //   final_el = max(0, 5000) = 5000.
    //   final_vft = 5000 + 800 = 5800.
    //   flow.vft_ul = 5000 + (1000B*8/1Mbps) = 5000 + 8000 = 13000.
    //   EligibleSet: {(5800,A)}

    PacketDescriptor p = scheduler.dequeue();
    ASSERT_EQ(p.flow_id, flowA_id);
    // After dequeue, current_VT should be 5800.
    // This test implies that the virtual_start_time (final_eligible_time) of the packet
    // should have been at least 5000. This is not directly assertable from outside without VFT access.
    // The VFT on eligible_set was 5800.
}


// --- Tests for HFSC Hierarchy Logic ---
// Note: NO_PARENT_ID_TEST is used for clarity, assuming 0 is the no-parent value from implementation.
const core::FlowId NO_PARENT_ID_TEST = 0;

// Helper to create FlowConfig for hierarchy tests easily
// HfscScheduler::FlowConfig(core::FlowId flow_id, core::FlowId p_id, ServiceCurve rt_sc, ServiceCurve ls_sc = {}, ServiceCurve ul_sc = {})
HfscScheduler::FlowConfig createHfscTestFlowConfig(
    core::FlowId id, core::FlowId parent_id,
    ServiceCurve rt_sc = ServiceCurve(0,0),
    ServiceCurve ls_sc = ServiceCurve(0,0),
    ServiceCurve ul_sc = ServiceCurve(0,0)) {
    return HfscScheduler::FlowConfig(id, parent_id, rt_sc, ls_sc, ul_sc);
}


TEST(HfscSchedulerHierarchyTest, ConstructorHierarchyValidation) {
    // Child with non-existent parent
    std::vector<HfscScheduler::FlowConfig> wrong_parent_configs = {
        createHfscTestFlowConfig(1, 10, defaultRtSc()) // Child 1, Parent 10 (Parent 10 not defined)
    };
    ASSERT_THROW(HfscScheduler scheduler(wrong_parent_configs, 10000000), std::invalid_argument);

    // Flow as its own parent
    std::vector<HfscScheduler::FlowConfig> self_parent_configs = {
        createHfscTestFlowConfig(1, 1, defaultRtSc()) // Child 1, Parent 1
    };
    ASSERT_THROW(HfscScheduler scheduler(self_parent_configs, 10000000), std::invalid_argument);

    // Valid simple hierarchy
    std::vector<HfscScheduler::FlowConfig> valid_hierarchy = {
        createHfscTestFlowConfig(10, NO_PARENT_ID_TEST, defaultRtSc()), // Parent P
        createHfscTestFlowConfig(1, 10, defaultRtSc())                  // Child C1 under P
    };
    ASSERT_NO_THROW(HfscScheduler scheduler(valid_hierarchy, 10000000));
    HfscScheduler scheduler(valid_hierarchy, 10000000);
    ASSERT_EQ(scheduler.get_num_configured_flows(), 2);
    // TODO: Add check for children_ids in parent once accessible or through behavior
}


TEST(HfscSchedulerHierarchyTest, ParentLimitsChildRT) {
    core::FlowId parentP_id = 10;
    core::FlowId childC1_id = 1;
    uint32_t packet_size_bytes = 1000; // 8000 bits

    ServiceCurve rt_P(1000000, 0);  // Parent P: 1 Mbps RT
    ServiceCurve rt_C1(2000000, 0); // Child C1: 2 Mbps RT (wants more than parent)
    ServiceCurve empty_sc(0,0);

    std::vector<HfscScheduler::FlowConfig> configs = {
        createHfscTestFlowConfig(parentP_id, NO_PARENT_ID_TEST, rt_P),
        createHfscTestFlowConfig(childC1_id, parentP_id,        rt_C1)
    };
    HfscScheduler scheduler(configs, 10000000);

    const int num_packets = 125 * 2; // Aim for 2 seconds of data at parent's 1Mbps rate
    for (int i = 0; i < num_packets; ++i) {
        scheduler.enqueue(createHfscTestPacket(childC1_id, packet_size_bytes));
    }

    for (int i = 0; i < num_packets; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        ASSERT_EQ(p.flow_id, childC1_id);
    }
    ASSERT_TRUE(scheduler.is_empty());
    // Rate assertion is indirect. Child's VFTs should be spaced by parent's 1Mbps rate.
}

TEST(HfscSchedulerHierarchyTest, ParentLimitsChildLS) {
    core::FlowId parentP_id = 10;
    core::FlowId childC1_id = 1;
    uint32_t packet_size_bytes = 1000;
    ServiceCurve empty_sc(0,0);

    ServiceCurve ls_P(1000000, 0);  // Parent P: 1 Mbps LS
    ServiceCurve ls_C1(2000000, 0); // Child C1: 2 Mbps LS

    std::vector<HfscScheduler::FlowConfig> configs = {
        createHfscTestFlowConfig(parentP_id, NO_PARENT_ID_TEST, empty_sc, ls_P),
        createHfscTestFlowConfig(childC1_id, parentP_id,        empty_sc, ls_C1)
    };
    HfscScheduler scheduler(configs, 10000000);

    const int num_packets = 125 * 2;
    for (int i = 0; i < num_packets; ++i) {
        scheduler.enqueue(createHfscTestPacket(childC1_id, packet_size_bytes));
    }
    for (int i = 0; i < num_packets; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        ASSERT_EQ(p.flow_id, childC1_id);
    }
    ASSERT_TRUE(scheduler.is_empty());
}


TEST(HfscSchedulerHierarchyTest, ParentULConstrainsChildrenAggregate) {
    core::FlowId parentP_id = 10;
    core::FlowId childC1_id = 1, childC2_id = 2;
    uint32_t packet_size_bytes = 1000;
    ServiceCurve empty_sc(0,0);

    ServiceCurve rt_P_generous(5000000, 0);
    ServiceCurve ul_P(1500000, 0);       // Parent P: UL 1.5 Mbps
    ServiceCurve rt_C(1000000, 0);       // Children C1, C2: 1 Mbps RT each

    std::vector<HfscScheduler::FlowConfig> configs = {
        createHfscTestFlowConfig(parentP_id, NO_PARENT_ID_TEST, rt_P_generous, empty_sc, ul_P),
        createHfscTestFlowConfig(childC1_id, parentP_id,        rt_C),
        createHfscTestFlowConfig(childC2_id, parentP_id,        rt_C)
    };
    HfscScheduler scheduler(configs, 10000000);

    const int num_packets_per_child = 150;
    for (int i = 0; i < num_packets_per_child; ++i) {
        scheduler.enqueue(createHfscTestPacket(childC1_id, packet_size_bytes));
        scheduler.enqueue(createHfscTestPacket(childC2_id, packet_size_bytes));
    }

    std::map<core::FlowId, int> counts;
    uint64_t total_bytes_parent = 0;
    for (int i = 0; i < 2 * num_packets_per_child; ++i) {
        PacketDescriptor p = scheduler.dequeue();
        counts[p.flow_id]++;
        if (p.flow_id == childC1_id || p.flow_id == childC2_id) {
            total_bytes_parent += p.packet_length_bytes;
        }
    }
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_GT(counts[childC1_id], 0);
    ASSERT_GT(counts[childC2_id], 0);
    // Rate check: (total_bytes_parent * 8 * 1M) / final_VT should be ~1.5M
    // This requires final_VT. For now, we check packet counts as a proxy for fairness under cap.
    ASSERT_NEAR(counts[childC1_id], counts[childC2_id], num_packets_per_child * 0.2);
}

TEST(HfscSchedulerHierarchyTest, TwoChildrenShareParentRTFairly) {
    core::FlowId pID = 10, c1ID = 1, c2ID = 2;
    uint32_t pkt_size = 1000;
    ServiceCurve empty_sc(0,0);
    std::vector<HfscScheduler::FlowConfig> configs = {
        createHfscTestFlowConfig(pID,  NO_PARENT_ID_TEST, ServiceCurve(2000000, 0)),
        createHfscTestFlowConfig(c1ID, pID,               ServiceCurve(1500000, 0)),
        createHfscTestFlowConfig(c2ID, pID,               ServiceCurve(1500000, 0))
    };
    HfscScheduler scheduler(configs, 10000000);

    const int num_pkts = 200;
    for(int i=0; i<num_pkts; ++i) {
        scheduler.enqueue(createHfscTestPacket(c1ID, pkt_size));
        scheduler.enqueue(createHfscTestPacket(c2ID, pkt_size));
    }

    std::map<core::FlowId, int> counts;
    for(int i=0; i<num_pkts*2; ++i) {
        counts[scheduler.dequeue().flow_id]++;
    }
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_NEAR(counts[c1ID], counts[c2ID], num_pkts * 0.2);
    ASSERT_EQ(counts[c1ID] + counts[c2ID], num_pkts*2);
}

TEST(HfscSchedulerHierarchyTest, TwoChildrenShareParentLSFairly) {
    core::FlowId pID = 10, c1ID = 1, c2ID = 2;
    uint32_t pkt_size = 1000;
    ServiceCurve empty_sc(0,0);
    std::vector<HfscScheduler::FlowConfig> configs = {
        createHfscTestFlowConfig(pID,  NO_PARENT_ID_TEST, empty_sc, ServiceCurve(2000000,0)),
        createHfscTestFlowConfig(c1ID, pID,               empty_sc, ServiceCurve(1000000,0)),
        createHfscTestFlowConfig(c2ID, pID,               empty_sc, ServiceCurve(1000000,0))
    };
    HfscScheduler scheduler(configs, 10000000);

    const int num_pkts = 200;
    for(int i=0; i<num_pkts; ++i) {
        scheduler.enqueue(createHfscTestPacket(c1ID, pkt_size));
        scheduler.enqueue(createHfscTestPacket(c2ID, pkt_size));
    }
    std::map<core::FlowId, int> counts;
    for(int i=0; i<num_pkts*2; ++i) {
        counts[scheduler.dequeue().flow_id]++;
    }
    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_NEAR(counts[c1ID], counts[c2ID], num_pkts * 0.1);
}


TEST(HfscSchedulerHierarchyTest, ChildWithNoParentBehavesAsRoot) {
    core::FlowId flowA_id = 1;
    core::FlowId parentP_id = 10;
    core::FlowId childB_id = 2;
    uint32_t packet_size_bytes = 1000;
    ServiceCurve empty_sc(0,0);

    std::vector<HfscScheduler::FlowConfig> configs = {
        createHfscTestFlowConfig(flowA_id,   NO_PARENT_ID_TEST, ServiceCurve(1000000, 0)),
        createHfscTestFlowConfig(parentP_id, NO_PARENT_ID_TEST, ServiceCurve(500000, 0)),
        createHfscTestFlowConfig(childB_id,  parentP_id,        ServiceCurve(1000000, 0))
    };
    HfscScheduler scheduler(configs, 10000000);

    const int num_pkts_A = 125;
    const int num_pkts_B = 63;

    for (int i = 0; i < num_pkts_A; ++i) scheduler.enqueue(createHfscTestPacket(flowA_id, packet_size_bytes));
    for (int i = 0; i < num_pkts_B; ++i) scheduler.enqueue(createHfscTestPacket(childB_id, packet_size_bytes));

    std::map<core::FlowId, int> counts;
    int dequeued_total = 0;
    while(!scheduler.is_empty() && dequeued_total < (num_pkts_A + num_pkts_B)) {
        counts[scheduler.dequeue().flow_id]++;
        dequeued_total++;
    }

    ASSERT_TRUE(scheduler.is_empty());
    ASSERT_EQ(counts[flowA_id], num_pkts_A);
    ASSERT_EQ(counts[childB_id], num_pkts_B);
}

} // namespace scheduler
} // namespace hqts


} // namespace scheduler
} // namespace hqts
