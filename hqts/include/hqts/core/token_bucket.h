#ifndef HQTS_CORE_TOKEN_BUCKET_H_
#define HQTS_CORE_TOKEN_BUCKET_H_

#include <cstdint>
#include <chrono>

namespace hqts {
namespace core {

class TokenBucket {
public:
    TokenBucket(uint64_t rate_bps, uint64_t capacity_bytes);

    bool consume(uint64_t tokens_to_consume);
    uint64_t available_tokens() const;
    bool is_conforming(uint64_t packet_size_bytes) const;

    void set_rate(uint64_t rate_bps);
    void set_capacity(uint64_t capacity_bytes);

private:
    void refill() const;

    uint64_t capacity_bytes_;
    mutable uint64_t tokens_bytes_;
    uint64_t rate_bps_;
    mutable std::chrono::steady_clock::time_point last_refill_time_;
};

} // namespace core
} // namespace hqts

#endif // HQTS_CORE_TOKEN_BUCKET_H_
