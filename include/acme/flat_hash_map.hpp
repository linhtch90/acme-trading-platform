#pragma once

#include <bit>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace acme {

// ============================================================================
// BEST PRACTICE: Open-Addressing Flat Hash Map (Zero Hot-Path Allocations)
// ============================================================================
// Standard `std::unordered_map` is notorious in low-latency C++ because:
//   1. It is a node-based container: every insert calls `operator new` on the heap.
//   2. Chaining pointers across buckets causes CPU L1/L2 cache misses.
//
// In high-frequency trading, we use an open-addressing flat hash table with
// linear probing and pre-allocated contiguous storage.
template <typename Key, typename Value>
class FlatHashMap {
public:
    explicit FlatHashMap(std::size_t min_capacity) {
        // Use a power-of-2 table size >= 2x min_capacity (load factor <= 0.5)
        const std::size_t target = std::bit_ceil(min_capacity * 2);
        const std::size_t cap    = target < 16 ? 16 : target;
        mask_ = cap - 1;
        buckets_.resize(cap);
    }

    FlatHashMap(const FlatHashMap&)            = delete;
    FlatHashMap& operator=(const FlatHashMap&) = delete;
    FlatHashMap(FlatHashMap&&) noexcept        = default;
    FlatHashMap& operator=(FlatHashMap&&) noexcept = default;
    ~FlatHashMap()                             = default;

    [[nodiscard]] bool insert(Key key, Value value) noexcept {
        if ((size_ + 1) * 2 > buckets_.size()) [[unlikely]] {
            return false; // Pre-allocated capacity exceeded
        }

        std::size_t idx = hash(key) & mask_;
        while (buckets_[idx].occupied) {
            if (buckets_[idx].key == key) [[unlikely]] {
                return false; // Duplicate key rejected
            }
            idx = (idx + 1) & mask_;
        }

        buckets_[idx].key      = key;
        buckets_[idx].value    = value;
        buckets_[idx].occupied = true;
        ++size_;
        return true;
    }

    [[nodiscard]] Value* find(Key key) noexcept {
        std::size_t idx = hash(key) & mask_;
        while (buckets_[idx].occupied) {
            if (buckets_[idx].key == key) {
                return &buckets_[idx].value;
            }
            idx = (idx + 1) & mask_;
        }
        return nullptr;
    }

    [[nodiscard]] const Value* find(Key key) const noexcept {
        std::size_t idx = hash(key) & mask_;
        while (buckets_[idx].occupied) {
            if (buckets_[idx].key == key) {
                return &buckets_[idx].value;
            }
            idx = (idx + 1) & mask_;
        }
        return nullptr;
    }

    bool erase(Key key) noexcept {
        std::size_t idx = hash(key) & mask_;
        while (buckets_[idx].occupied) {
            if (buckets_[idx].key == key) {
                // Backward-shift deletion (avoids tombstones degrading linear probe performance over time)
                buckets_[idx].occupied = false;
                --size_;
                shift_backward(idx);
                return true;
            }
            idx = (idx + 1) & mask_;
        }
        return false;
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

private:
    struct Bucket {
        Key   key{};
        Value value{};
        bool  occupied{false};
    };

    // Fibonacci multiplicative hashing for 64-bit integer keys
    [[nodiscard]] static constexpr std::size_t hash(Key k) noexcept {
        std::uint64_t x = static_cast<std::uint64_t>(k);
        x ^= x >> 33U;
        x *= 0xff51afd7ed558ccdULL;
        x ^= x >> 33U;
        return static_cast<std::size_t>(x);
    }

    void shift_backward(std::size_t empty_idx) noexcept {
        std::size_t curr = (empty_idx + 1) & mask_;
        while (buckets_[curr].occupied) {
            const std::size_t ideal = hash(buckets_[curr].key) & mask_;
            // Check if `empty_idx` lies cyclically between `ideal` and `curr`
            const bool should_move = (curr >= ideal)
                ? (ideal <= empty_idx && empty_idx < curr)
                : (ideal <= empty_idx || empty_idx < curr);

            if (should_move) {
                buckets_[empty_idx] = buckets_[curr];
                buckets_[curr].occupied = false;
                empty_idx = curr;
            }
            curr = (curr + 1) & mask_;
        }
    }

    std::vector<Bucket> buckets_;
    std::size_t         mask_{0};
    std::size_t         size_{0};
};

} // namespace acme
