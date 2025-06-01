#include "gtest/gtest.h"
#include "hqts/scheduler/aqm_queue.h"
#include "hqts/scheduler/packet_descriptor.h" // For PacketDescriptor
#include "hqts/core/flow_context.h"          // For core::FlowId (used in PacketDescriptor)

#include <vector>
#include <numeric>   // For std::accumulate
#include <cmath>     // For std::abs, std::pow
#include <stdexcept> // For std::invalid_argument etc.

namespace hqts {
namespace scheduler {

// Helper to create a PacketDescriptor for AQM tests. Priority field is not used by RedAqmQueue.
PacketDescriptor createAqmTestPacket(core::FlowId flow_id, uint32_t length, size_t payload_size = 0) {
    return PacketDescriptor(flow_id, length, 0 /* priority unused */, payload_size);
}

// Default parameters for many tests, can be overridden
RedAqmParameters defaultRedParams(
    uint32_t capacity_pkts = 100, uint32_t pkt_size = 100, // For byte conversion
    double min_thresh_factor = 0.2,
    double max_thresh_factor = 0.6,
    double max_p = 0.1,
    double ewma_w = 0.002) {
    uint32_t capacity_bytes = capacity_pkts * pkt_size;
    return RedAqmParameters(
        static_cast<uint32_t>(capacity_bytes * min_thresh_factor),
        static_cast<uint32_t>(capacity_bytes * max_thresh_factor),
        max_p,
        ewma_w,
        capacity_bytes
    );
}


TEST(RedAqmQueueTest, ConstructorAndParameterValidation) {
    // Valid parameters
    ASSERT_NO_THROW(RedAqmParameters(1000, 2000, 0.1, 0.002, 3000));

    // Invalid: min_threshold >= max_threshold
    ASSERT_THROW(RedAqmParameters(2000, 1000, 0.1, 0.002, 3000), std::invalid_argument);
    ASSERT_THROW(RedAqmParameters(1000, 1000, 0.1, 0.002, 3000), std::invalid_argument);

    // Invalid: max_threshold > capacity
    ASSERT_THROW(RedAqmParameters(1000, 3001, 0.1, 0.002, 3000), std::invalid_argument);

    // Invalid: zero thresholds or capacity
    ASSERT_THROW(RedAqmParameters(0, 2000, 0.1, 0.002, 3000), std::invalid_argument);
    ASSERT_THROW(RedAqmParameters(1000, 0, 0.1, 0.002, 3000), std::invalid_argument);
    ASSERT_THROW(RedAqmParameters(1000, 2000, 0.1, 0.002, 0), std::invalid_argument);

    // Invalid: ewma_weight
    ASSERT_THROW(RedAqmParameters(1000, 2000, 0.1, 0.0, 3000), std::invalid_argument); // Correct: weight <= 0.0
    ASSERT_NO_THROW(RedAqmParameters(1000, 2000, 0.1, 1.0, 3000)); // Correct: weight == 1.0 is now allowed
    ASSERT_THROW(RedAqmParameters(1000, 2000, 0.1, -0.1, 3000), std::invalid_argument); // Correct: weight <= 0.0
    ASSERT_THROW(RedAqmParameters(1000, 2000, 0.1, 1.1, 3000), std::invalid_argument); // Correct: weight > 1.0

    // Invalid: max_probability
    ASSERT_THROW(RedAqmParameters(1000, 2000, 0.0, 0.002, 3000), std::invalid_argument);
    ASSERT_THROW(RedAqmParameters(1000, 2000, -0.1, 0.002, 3000), std::invalid_argument);
    ASSERT_THROW(RedAqmParameters(1000, 2000, 1.1, 0.002, 3000), std::invalid_argument);

    RedAqmParameters params = defaultRedParams();
    RedAqmQueue queue(params);
    ASSERT_TRUE(queue.is_empty());
    ASSERT_EQ(queue.get_current_packet_count(), 0);
    ASSERT_EQ(queue.get_current_byte_size(), 0);
    ASSERT_DOUBLE_EQ(queue.get_average_queue_size_bytes(), 0.0);

    const RedAqmParameters& retrieved_params = queue.get_parameters();
    ASSERT_EQ(retrieved_params.min_threshold_bytes, params.min_threshold_bytes);
}

TEST(RedAqmQueueTest, EnqueueDequeueEmpty) {
    RedAqmQueue queue(defaultRedParams());
    PacketDescriptor p1 = createAqmTestPacket(1, 100);

    ASSERT_TRUE(queue.enqueue(p1));
    ASSERT_FALSE(queue.is_empty());
    ASSERT_EQ(queue.get_current_packet_count(), 1);
    ASSERT_EQ(queue.get_current_byte_size(), 100);

    PacketDescriptor p_out = queue.dequeue();
    ASSERT_EQ(p_out.flow_id, p1.flow_id);
    ASSERT_EQ(p_out.packet_length_bytes, p1.packet_length_bytes);

    ASSERT_TRUE(queue.is_empty());
    ASSERT_EQ(queue.get_current_packet_count(), 0);
    ASSERT_EQ(queue.get_current_byte_size(), 0);
    ASSERT_THROW(queue.dequeue(), std::runtime_error);
}

TEST(RedAqmQueueTest, PhysicalCapacityDrop) {
    RedAqmParameters params(200, 400, 0.1, 0.002, 500); // Capacity 500 bytes
    RedAqmQueue queue(params);

    ASSERT_TRUE(queue.enqueue(createAqmTestPacket(1, 200))); // total 200
    ASSERT_TRUE(queue.enqueue(createAqmTestPacket(2, 200))); // total 400
    ASSERT_EQ(queue.get_current_byte_size(), 400);

    // This packet (150B) should exceed capacity (400+150 > 500)
    ASSERT_FALSE(queue.enqueue(createAqmTestPacket(3, 150)));
    ASSERT_EQ(queue.get_current_byte_size(), 400); // Still 400
    ASSERT_EQ(queue.get_current_packet_count(), 2);

    // This packet (100B) should fit
    ASSERT_TRUE(queue.enqueue(createAqmTestPacket(4, 100)));
    ASSERT_EQ(queue.get_current_byte_size(), 500);
    ASSERT_EQ(queue.get_current_packet_count(), 3);

    // This packet (1B) should not fit
    ASSERT_FALSE(queue.enqueue(createAqmTestPacket(5, 1)));
    ASSERT_EQ(queue.get_current_byte_size(), 500);
}

TEST(RedAqmQueueTest, EwmaAverageCalculation) {
    // Params: min=200, max=800, cap=1000, weight=0.5
    RedAqmParameters params(200, 800, 0.1, 0.5, 1000);
    RedAqmQueue queue(params);
    double current_avg;

    // Initial: avg=0, total_bytes=0
    // Enqueue P1 (100B)
    //  update_avg (uses total_bytes=0): avg = (1-0.5)*0 + 0.5*0 = 0.
    //  (P1 not dropped). Add P1. total_bytes=100.
    queue.enqueue(createAqmTestPacket(1, 100));
    current_avg = queue.get_average_queue_size_bytes(); // This avg is *after* first update_avg for P1
    ASSERT_DOUBLE_EQ(current_avg, 0.0); // Avg calc based on queue *before* P1

    // Enqueue P2 (100B)
    //  update_avg (uses total_bytes=100): avg = (1-0.5)*0 + 0.5*100 = 50.
    //  (P2 not dropped). Add P2. total_bytes=200.
    queue.enqueue(createAqmTestPacket(2, 100));
    current_avg = queue.get_average_queue_size_bytes(); // Avg based on queue *before* P2
    ASSERT_DOUBLE_EQ(current_avg, 50.0);

    // Enqueue P3 (100B)
    //  update_avg (uses total_bytes=200): avg = (1-0.5)*50 + 0.5*200 = 25 + 100 = 125.
    //  (P3 not dropped). Add P3. total_bytes=300.
    queue.enqueue(createAqmTestPacket(3, 100));
    current_avg = queue.get_average_queue_size_bytes();
    ASSERT_DOUBLE_EQ(current_avg, 125.0);

    // Dequeue P1 (100B)
    //  P1 removed. total_bytes=200.
    //  update_avg (uses total_bytes=200): avg = (1-0.5)*125 + 0.5*200 = 62.5 + 100 = 162.5.
    queue.dequeue();
    current_avg = queue.get_average_queue_size_bytes();
    ASSERT_DOUBLE_EQ(current_avg, 162.5);
}

TEST(RedAqmQueueTest, RedDropsBelowMinThreshold) {
    // min_thresh=1000, max_thresh=2000, capacity=3000
    RedAqmParameters params(1000, 2000, 0.1, 0.002, 3000);
    RedAqmQueue queue(params);

    int packets_enqueued = 0;
    for (int i = 0; i < 5; ++i) { // Enqueue 5 * 100 = 500 bytes
        if(queue.enqueue(createAqmTestPacket(i, 100))) {
            packets_enqueued++;
        }
    }
    ASSERT_EQ(packets_enqueued, 5);
    // Avg queue size should be < 1000 (actual is 500, EWMA will be less initially)
    ASSERT_LT(queue.get_average_queue_size_bytes(), params.min_threshold_bytes);
    // packets_since_last_drop_ should be 5 (if internal counter accessible)
}

TEST(RedAqmQueueTest, RedDropsAtOrAboveMaxThreshold) {
    // min=200, max=400, max_p=0.1, capacity=1000. weight=1.0 (so avg = current)
    RedAqmParameters params(200, 400, 0.1, 1.0, 1000);
    RedAqmQueue queue(params);

    // Fill queue to average >= max_threshold
    // After 3 packets (300 bytes), avg for 4th packet decision is 200.0 (if w=1)
    // After 4 packets (400 bytes), avg for 5th packet decision is 300.0 (if w=1)
    // The params.max_threshold_bytes is 400.
    // So, to make avg for NEXT packet decision be >= max_threshold, we need current_total_bytes to be 400.
    for(int i=0; i<4; ++i) queue.enqueue(createAqmTestPacket(i,100)); // current_total_bytes = 400
    // The average calculated when the 4th packet was enqueued was based on 300 bytes.
    ASSERT_EQ(queue.get_average_queue_size_bytes(), 300.0);
    // For the 5th packet's decision, update_average_queue_size() will use current_total_bytes=400.
    // So avg becomes (1-1)*300 + 1*400 = 400. This hits max_threshold.

    // Now, probability p_b = max_p. Gentle RED makes actual prob higher.
    // With count starting at 0 after a potential drop, or high if no drops.
    // If packets_since_last_drop_ * max_p >= 1, it's a forced drop.
    // 1 / 0.1 = 10. So after 10 successful enqueues in this state, next is guaranteed drop.

    int drops = 0;
    int attempts = 200; // Multiple packets to observe drops
    // After filling to max_thresh, packets_since_last_drop_ is 4.
    // p_b = 0.1.
    // Pkt 5: count=4. den=1-4*0.1=0.6. dp=0.1/0.6 = 0.166.
    // Pkt 6: count=5 (if 5 not dropped). den=1-5*0.1=0.5. dp=0.1/0.5 = 0.2.
    // ...
    // Pkt 10: count=9. den=1-9*0.1=0.1. dp=0.1/0.1 = 1.0. (Guaranteed drop if reached)

    for (int i = 0; i < attempts; ++i) {
        if (!queue.enqueue(createAqmTestPacket(100 + i, 10))) { // Small packets
            drops++;
        }
    }
    // Expect significant drops. The exact number is probabilistic.
    // At least one drop should occur due to count escalating to force drop.
    ASSERT_GT(drops, 0);
    // More specifically, after enough non-drops, count * p_b will make denominator <=0, forcing drop.
    // Max non-drops before forced drop: count < 1/p_b. count < 10. So at 10th attempt, drop is guaranteed.
}


TEST(RedAqmQueueTest, RedDropsBetweenMinMaxThreshold) {
    // min=200, max=800 (range 600), max_p=0.1, capacity=1000. weight=1.0 (avg = current)
    RedAqmParameters params(200, 800, 0.1, 1.0, 1000);
    RedAqmQueue queue(params);

    // Fill queue to current_total_bytes = 500. Avg for next packet decision will be 500.
    for(int i=0; i<5; ++i) queue.enqueue(createAqmTestPacket(i,100)); // current_total_bytes = 500
    // Avg calculated when 5th packet was enqueued was based on 400 bytes.
    ASSERT_DOUBLE_EQ(queue.get_average_queue_size_bytes(), 400.0);
    // For the 6th packet's decision, update_average_queue_size() will use current_total_bytes=500.
    // So avg becomes (1-1)*400 + 1*500 = 500. This is between min and max.

    // At avg=500, p_b = max_p * (500-200)/(800-200) = 0.1 * (300/600) = 0.1 * 0.5 = 0.05
    // packets_since_last_drop_ is 5.
    // dp for next packet = 0.05 / (1 - 5 * 0.05) = 0.05 / (1 - 0.25) = 0.05 / 0.75 = ~0.0667

    int drops = 0;
    int attempts = 1000;
    for (int i = 0; i < attempts; ++i) {
        // Enqueue small packets to minimally change average, to test prob at this level
        if (!queue.enqueue(createAqmTestPacket(100 + i, 1))) {
            drops++;
        }
        // To keep avg somewhat stable, dequeue if too full (not ideal for this test)
        if (queue.get_current_byte_size() > 550) queue.dequeue();
    }
    // Expected drops around attempts * effective_prob. This is very rough.
    // For p_b=0.05, count=0 -> dp=0.05. For count=10, den=1-10*0.05=0.5, dp=0.1
    // The average dp will be higher than p_b.
    ASSERT_GT(drops, attempts * 0.05 * 0.5); // Expect at least some drops, very roughly > half of base prob
    ASSERT_LT(drops, attempts * 0.2); // And not excessively high, very roughly < 2 * escalated prob
}

TEST(RedAqmQueueTest, GentleRedEffectOfCount) {
    // min=100, max=1100 (range 1000), max_p=0.1, capacity=2000. weight=1.0
    RedAqmParameters params(100, 1100, 0.1, 1.0, 2000);
    RedAqmQueue queue(params);

    // Fill to current_total_bytes = 600. Avg for next packet decision will be 600.
    for(int i=0; i<6; ++i) queue.enqueue(createAqmTestPacket(i,100)); // current_total_bytes = 600
    // Avg calculated when 6th packet was enqueued was based on 500 bytes.
    ASSERT_DOUBLE_EQ(queue.get_average_queue_size_bytes(), 500.0);
    // For the 7th packet's decision, update_average_queue_size() will use current_total_bytes=600.
    // So avg becomes (1-1)*500 + 1*600 = 600.
    // p_b = 0.1 * (600-100)/(1100-100) = 0.1 * 500/1000 = 0.05.
    // packets_since_last_drop_ will be 6 (from the 6 enqueues).

    // Next enqueue attempt:
    // count = 6. denom = 1 - 6 * 0.05 = 1 - 0.3 = 0.7.
    // dp = 0.05 / 0.7 = ~0.0714.

    // If we enqueue successfully for a few more times:
    // count=7: dp = 0.05 / (1-7*0.05) = 0.05 / 0.65 = ~0.0769
    // count=8: dp = 0.05 / (1-8*0.05) = 0.05 / 0.6 = ~0.0833
    // count=9: dp = 0.05 / (1-9*0.05) = 0.05 / 0.55 = ~0.0909
    // count=10: dp = 0.05 / (1-10*0.05) = 0.05 / 0.5 = 0.1
    // count=19: dp = 0.05 / (1-19*0.05) = 0.05 / 0.05 = 1.0 (guaranteed drop)

    bool dropped_at_high_count = false;
    for (int i = 0; i < 20; ++i) { // Try to enqueue 20 more packets
        if (!queue.enqueue(createAqmTestPacket(100+i, 10))) { // Small packets
            dropped_at_high_count = true;
            ASSERT_GE(i, 9); // Should take at least 10-ish tries from count=6 to force drop if random is unlucky
            ASSERT_LE(i, 13); // (6 existing + 13 new = 19) -> guaranteed drop at 19th non-drop since last.
            break;
        }
        if (i == 13 && !dropped_at_high_count) { // If by 13th new packet (total count 19) no drop, fail
             // This means packets_since_last_drop_ would be 6+13 = 19.
             // Denom = 1 - 19 * 0.05 = 1 - 0.95 = 0.05. dp = 0.05 / 0.05 = 1.0
             // So, it MUST have dropped by now.
        }
    }
    ASSERT_TRUE(dropped_at_high_count); // Ensure a drop eventually happened due to escalating probability

    // Check reset of packets_since_last_drop_
    // This requires internal access or a way to guarantee a drop then check.
    // The above loop implies it if it dropped then continued.
    // For a more direct test, one might need a friend class or specific test methods.
}


} // namespace scheduler
} // namespace hqts
