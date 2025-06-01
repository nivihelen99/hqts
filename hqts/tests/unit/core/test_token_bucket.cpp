#include "gtest/gtest.h"
#include "hqts/core/token_bucket.h"

#include <chrono>
#include <thread> // For std::this_thread::sleep_for
#include <cmath>  // For std::abs for ASSERT_NEAR comparisons

// Helper function for sleeping, can be put in a common test utility if needed
void sleep_ms(int milliseconds) {
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

// Define a tolerance for token comparisons in time-based tests
// This accounts for minor inaccuracies in sleep timing and integer division in refill.
const uint64_t TOKEN_TOLERANCE = 10; // Allow a small variance in tokens

namespace hqts {
namespace core {

class TokenBucketTest : public ::testing::Test {
protected:
    // You can define helper functions or common setup here if needed
    // For example, a helper to calculate expected tokens with tolerance
    void expect_tokens_near(uint64_t actual, uint64_t expected, uint64_t tolerance = TOKEN_TOLERANCE) {
        EXPECT_GE(actual, expected - tolerance);
        EXPECT_LE(actual, expected + tolerance);
    }
};

TEST_F(TokenBucketTest, InitialState) {
    TokenBucket tb(8000, 1000); // 1000 bytes/sec, 1000 bytes capacity
    ASSERT_EQ(tb.available_tokens(), 1000);
    ASSERT_TRUE(tb.is_conforming(1000));
    ASSERT_FALSE(tb.is_conforming(1001));
    // is_conforming should not consume tokens
    ASSERT_EQ(tb.available_tokens(), 1000);
}

TEST_F(TokenBucketTest, BasicConsumption) {
    TokenBucket tb(8000, 1000); // 1000 bytes/sec, 1000 bytes capacity
    ASSERT_TRUE(tb.consume(100));
    ASSERT_EQ(tb.available_tokens(), 900);

    ASSERT_TRUE(tb.consume(900));
    ASSERT_EQ(tb.available_tokens(), 0);

    ASSERT_FALSE(tb.consume(1));
    ASSERT_EQ(tb.available_tokens(), 0);
}

TEST_F(TokenBucketTest, RateLimitingSimple) {
    uint64_t rate_bps = 8000; // 1000 bytes per second
    uint64_t capacity_bytes = 1000;
    TokenBucket tb(rate_bps, capacity_bytes);

    // Consume all tokens
    ASSERT_TRUE(tb.consume(capacity_bytes));
    ASSERT_EQ(tb.available_tokens(), 0);

    // Wait for 100 ms
    sleep_ms(100);

    // Expected tokens: (100_ms * 8000_bps) / (8 * 1000_ms_in_sec) = 100 bytes
    // (100 * 1000 us * 8000 bps) / (8 * 1000000 us_in_sec) = 100 bytes
    // Calculation in TokenBucket: (elapsed_microseconds * rate_bps_) / (8 * 1000000);
    // (100000 * 8000) / 8000000 = 100
    uint64_t expected_tokens = (100 * 1000 * rate_bps) / (8 * 1000000);
    expect_tokens_near(tb.available_tokens(), expected_tokens);

    ASSERT_TRUE(tb.consume(expected_tokens - TOKEN_TOLERANCE > 0 ? expected_tokens - TOKEN_TOLERANCE : 0)); // consume what should be there
}

TEST_F(TokenBucketTest, RateLimitingOverTime) {
    uint64_t rate_bps = 80000; // 10000 bytes per second
    uint64_t capacity_bytes = 20000;
    TokenBucket tb(rate_bps, capacity_bytes);

    ASSERT_TRUE(tb.consume(capacity_bytes)); // Empty the bucket
    ASSERT_EQ(tb.available_tokens(), 0);

    const int iterations = 10; // 10 * 200ms = 2 seconds
    const int sleep_duration_ms = 200;
    const uint64_t bytes_per_second = rate_bps / 8;

    for (int i = 0; i < iterations; ++i) {
        sleep_ms(sleep_duration_ms);
        // Expected tokens for this 200ms interval
        uint64_t expected_tokens_this_interval = (static_cast<uint64_t>(sleep_duration_ms) * 1000 * rate_bps) / (8 * 1000000);

        uint64_t current_tokens = tb.available_tokens();
        expect_tokens_near(current_tokens, expected_tokens_this_interval);

        if (current_tokens > 0) {
             // Try to consume slightly less than expected to avoid flakiness due to timing precision for the available_tokens() call itself
            uint64_t to_consume = (current_tokens > TOKEN_TOLERANCE/2) ? current_tokens - TOKEN_TOLERANCE/2 : current_tokens;
            if(to_consume > 0) { // only consume if there's something meaningful
                ASSERT_TRUE(tb.consume(to_consume));
            }
        } else {
            // If no tokens generated, it might be an issue or too short interval for integer math
            // For this test, we expect some tokens.
            ASSERT_GT(expected_tokens_this_interval, 0) << "No tokens expected, check test params.";
            // If we expected tokens but got none, this might be an issue, let's see if available_tokens was just slow
            // but for this test, we'll assume consume would also fail if current_tokens was 0.
        }
    }
}


TEST_F(TokenBucketTest, BurstCapacity) {
    uint64_t rate_bps = 8000; // 1000 bytes/sec
    uint64_t capacity_bytes = 500;
    TokenBucket tb(rate_bps, capacity_bytes);

    ASSERT_EQ(tb.available_tokens(), capacity_bytes);
    ASSERT_TRUE(tb.consume(capacity_bytes));
    ASSERT_EQ(tb.available_tokens(), 0);
    ASSERT_FALSE(tb.consume(1));

    // Wait long enough to generate more tokens than capacity
    // e.g., capacity is 500, rate is 1000 bytes/sec.
    // To generate 600 tokens (more than capacity), it would take 600ms.
    sleep_ms(600);

    ASSERT_EQ(tb.available_tokens(), capacity_bytes); // Should be capped at capacity
}

TEST_F(TokenBucketTest, SetRateTest) {
    uint64_t initial_rate_bps = 8000; // 1000 bytes/sec
    uint64_t capacity_bytes = 2000;
    TokenBucket tb(initial_rate_bps, capacity_bytes);

    ASSERT_TRUE(tb.consume(500)); // Consume some tokens, 1500 left
    ASSERT_EQ(tb.available_tokens(), 1500);

    sleep_ms(100); // 100ms * 1000 bytes/sec = 100 bytes. Available should be 1500 + 100 = 1600
    uint64_t expected_after_sleep1 = 1500 + ((100 * 1000 * initial_rate_bps) / (8 * 1000000));
    // available_tokens calls refill, so it's already updated
    expect_tokens_near(tb.available_tokens(), expected_after_sleep1);

    uint64_t tokens_before_set_rate = tb.available_tokens();

    uint64_t new_rate_bps = 16000; // 2000 bytes/sec
    tb.set_rate(new_rate_bps);

    // available_tokens() was called before set_rate, which called refill().
    // set_rate() calls refill() again. last_refill_time_ would be very recent.
    // So, tokens should be largely unchanged by the set_rate call itself.
    expect_tokens_near(tb.available_tokens(), tokens_before_set_rate);


    // Wait for another 100 ms. Tokens should generate based on the new rate.
    sleep_ms(100);
    // Expected from new rate: 100ms * 2000 bytes/sec = 200 bytes
    uint64_t expected_new_tokens = (100 * 1000 * new_rate_bps) / (8 * 1000000);
    uint64_t expected_total_after_sleep2 = std::min(capacity_bytes, tokens_before_set_rate + expected_new_tokens);

    expect_tokens_near(tb.available_tokens(), expected_total_after_sleep2);
}


TEST_F(TokenBucketTest, SetCapacityTest) {
    uint64_t rate_bps = 8000; // 1000 bytes/sec
    uint64_t initial_capacity_bytes = 1000;
    TokenBucket tb(rate_bps, initial_capacity_bytes);

    ASSERT_EQ(tb.available_tokens(), initial_capacity_bytes);

    tb.set_capacity(500); // Lower capacity
    ASSERT_EQ(tb.available_tokens(), 500); // Tokens should be capped

    // Consume some to make it less than new capacity, e.g. 300 tokens left
    ASSERT_TRUE(tb.consume(200));
    ASSERT_EQ(tb.available_tokens(), 300);

    tb.set_capacity(1500); // Higher capacity
    ASSERT_EQ(tb.available_tokens(), 300); // Tokens don't magically increase

    // Wait for tokens to regenerate (e.g., 500 ms, so 500 tokens)
    // Expected: 300 + 500 = 800. Still less than new capacity of 1500.
    sleep_ms(500);
    uint64_t expected_tokens = 300 + ((500 * 1000 * rate_bps) / (8 * 1000000));
    expect_tokens_near(tb.available_tokens(), std::min(1500UL, expected_tokens));

    // Wait long enough to fill to new capacity (e.g. 2 seconds total = 2000 tokens)
    // Current is ~800. Need 700 more. 700ms should be enough.
    sleep_ms(1200); // (Previously 500ms, total 1700ms from 300 tokens state)
                    // 300 + 1700 * 1000B/s = 300 + 1700 = 2000. Capped at 1500.
    expect_tokens_near(tb.available_tokens(), 1500);
}


TEST_F(TokenBucketTest, ZeroRate) {
    TokenBucket tb(0, 1000); // 0 rate, 1000 capacity
    ASSERT_EQ(tb.available_tokens(), 1000);
    ASSERT_TRUE(tb.consume(1000));
    ASSERT_EQ(tb.available_tokens(), 0);

    sleep_ms(100); // Wait for a period

    ASSERT_EQ(tb.available_tokens(), 0); // Still zero tokens
    ASSERT_FALSE(tb.consume(1));
}

TEST_F(TokenBucketTest, ZeroCapacity) {
    TokenBucket tb(8000, 0); // 1000 bytes/sec rate, 0 capacity
    ASSERT_EQ(tb.available_tokens(), 0);

    // Consuming 0 tokens should ideally be a no-op and succeed.
    // Depending on interpretation, if available is 0, consume(0) is true.
    ASSERT_TRUE(tb.consume(0));
    ASSERT_EQ(tb.available_tokens(), 0);

    ASSERT_FALSE(tb.consume(1));
    ASSERT_EQ(tb.available_tokens(), 0);

    sleep_ms(100); // Wait for a period
    ASSERT_EQ(tb.available_tokens(), 0); // Still zero, as capacity is zero
}

TEST_F(TokenBucketTest, IsConformingNoSideEffects) {
    TokenBucket tb(8000, 1000); // 1000 bytes/sec, 1000 bytes capacity

    // Initial state: 1000 tokens
    ASSERT_EQ(tb.available_tokens(), 1000);

    ASSERT_TRUE(tb.is_conforming(500));
    ASSERT_EQ(tb.available_tokens(), 1000); // Should not change

    ASSERT_TRUE(tb.is_conforming(1000));
    ASSERT_EQ(tb.available_tokens(), 1000); // Should not change

    ASSERT_FALSE(tb.is_conforming(1001));
    ASSERT_EQ(tb.available_tokens(), 1000); // Should not change

    // Consume some tokens
    ASSERT_TRUE(tb.consume(200)); // 800 left
    ASSERT_EQ(tb.available_tokens(), 800);

    ASSERT_TRUE(tb.is_conforming(800));
    ASSERT_EQ(tb.available_tokens(), 800);

    ASSERT_FALSE(tb.is_conforming(801));
    ASSERT_EQ(tb.available_tokens(), 800);
}

// Test with very high rate to check for overflow issues if any (though uint64_t should be fine)
TEST_F(TokenBucketTest, HighRateRefill) {
    uint64_t high_rate_bps = 8ULL * 1000000 * 2000; // 2000 Bytes per microsecond effectively, or 2GB/s
    uint64_t capacity_bytes = 50000; // 50KB
    TokenBucket tb(high_rate_bps, capacity_bytes);

    ASSERT_TRUE(tb.consume(capacity_bytes));
    ASSERT_EQ(tb.available_tokens(), 0);

    sleep_ms(10); // 10 ms = 10000 microseconds
    // Expected: 10000 us * (high_rate_bps / (8 * 1000000))
    // = 10000 us * 2000 B/us = 20,000,000 Bytes. This will be capped by capacity.
    // So, after 10ms, it should be full (50000 bytes)

    // Refill happens on available_tokens()
    expect_tokens_near(tb.available_tokens(), capacity_bytes);
}

TEST_F(TokenBucketTest, ConsumeZeroTokens) {
    TokenBucket tb(8000, 100);
    ASSERT_EQ(tb.available_tokens(), 100);
    ASSERT_TRUE(tb.consume(0)); // Consuming 0 should succeed
    ASSERT_EQ(tb.available_tokens(), 100); // Tokens should remain unchanged

    // Empty the bucket
    ASSERT_TRUE(tb.consume(100));
    ASSERT_EQ(tb.available_tokens(), 0);
    ASSERT_TRUE(tb.consume(0)); // Consuming 0 should still succeed
    ASSERT_EQ(tb.available_tokens(), 0); // Tokens should remain unchanged
}


} // namespace core
} // namespace hqts
