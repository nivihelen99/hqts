#include "hqts/core/token_bucket.h"

#include <algorithm> // For std::min
#include <chrono>    // For std::chrono

namespace hqts {
namespace core {

TokenBucket::TokenBucket(uint64_t rate_bps, uint64_t capacity_bytes)
    : capacity_bytes_(capacity_bytes),
      tokens_bytes_(capacity_bytes), // Initially full
      rate_bps_(rate_bps),
      last_refill_time_(std::chrono::steady_clock::now()) {}

void TokenBucket::refill() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed_microseconds = std::chrono::duration_cast<std::chrono::microseconds>(now - last_refill_time_).count();

    // Calculate new tokens: (elapsed_microseconds * rate_bps_) / (bits_in_byte * microseconds_in_second)
    // (elapsed_microseconds * rate_bps_) / (8 * 1000000)
    if (elapsed_microseconds > 0) { // Avoid division by zero or negative time
        uint64_t new_tokens = (static_cast<uint64_t>(elapsed_microseconds) * rate_bps_) / (8 * 1000000);
        if (new_tokens > 0) {
            tokens_bytes_ = std::min(capacity_bytes_, tokens_bytes_ + new_tokens);
        }
    }
    last_refill_time_ = now;
}

bool TokenBucket::consume(uint64_t tokens_to_consume) {
    refill();
    if (tokens_bytes_ >= tokens_to_consume) {
        tokens_bytes_ -= tokens_to_consume;
        return true;
    }
    return false;
}

uint64_t TokenBucket::available_tokens() const {
    refill();
    return tokens_bytes_;
}

bool TokenBucket::is_conforming(uint64_t packet_size_bytes) const {
    refill();
    return tokens_bytes_ >= packet_size_bytes;
}

void TokenBucket::set_rate(uint64_t rate_bps) {
    refill(); // Update tokens based on the old rate before changing it
    rate_bps_ = rate_bps;
}

void TokenBucket::set_capacity(uint64_t capacity_bytes) {
    refill(); // Update tokens based on current state
    capacity_bytes_ = capacity_bytes;
    tokens_bytes_ = std::min(tokens_bytes_, capacity_bytes_); // Ensure tokens do not exceed new capacity
}

} // namespace core
} // namespace hqts
