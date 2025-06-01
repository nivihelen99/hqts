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


} // namespace scheduler
} // namespace hqts
