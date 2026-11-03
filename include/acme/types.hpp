#pragma once

#include <cstdint>
#include <string_view>

namespace acme {

// ============================================================================
// LESSON 1: Exact-Width Integers & Fixed-Point Arithmetic
// ============================================================================
// In trading systems, NEVER use float or double for order prices!
// Floating-point numbers cannot represent 0.01 + 0.02 == 0.03 exactly, and
// floating-point operations are slower to compare/hash than integers.
//
// Instead, exchanges (like NASDAQ ITCH or CME MDP3) send prices as integers
// in "ticks" or scaled by 10^4 (e.g., $150.2500 -> 1,502,500).
using OrderId   = std::uint64_t;
using Price     = std::int64_t;   // Signed so price differences (spreads/PnL) don't underflow
using Quantity  = std::uint32_t;
using Timestamp = std::uint64_t;  // Nanoseconds since epoch or CPU TSC ticks

constexpr Price PRICE_SCALE = 10'000; // 4 decimal places (C++14 digit separator ')

[[nodiscard]] constexpr Price to_ticks(double human_price) noexcept {
    return static_cast<Price>(human_price * PRICE_SCALE + (human_price >= 0 ? 0.5 : -0.5));
}

[[nodiscard]] constexpr double to_human_price(Price ticks) noexcept {
    return static_cast<double>(ticks) / static_cast<double>(PRICE_SCALE);
}

// ============================================================================
// LESSON 2: Scoped Enums (enum class) with Explicit Underlying Type
// ============================================================================
// Specifying `: std::uint8_t` keeps the enum down to 1 byte instead of 4 bytes,
// allowing tighter struct packing and better CPU cache utilization.
enum class Side : std::uint8_t {
    Buy  = 0,
    Sell = 1
};

enum class OrderType : std::uint8_t {
    Limit  = 0, // Rests in the book if not immediately matched
    Market = 1, // Matches immediately at best available price(s); never rests
    IOC    = 2  // Immediate-Or-Cancel: match what you can at limit price, cancel remainder
};

[[nodiscard]] constexpr std::string_view to_string(Side side) noexcept {
    return side == Side::Buy ? "BUY" : "SELL";
}

// ============================================================================
// LESSON 3: Struct Packing & Cache-Line Awareness
// ============================================================================
// A modern x86_64 CPU cache line is 64 bytes.
// Keeping hot structs compact and aligned prevents splitting reads across
// multiple cache lines.
struct Trade {
    OrderId  maker_order_id{}; // The resting order that provided liquidity
    OrderId  taker_order_id{}; // The incoming aggressive order that crossed the spread
    Price    price{};          // Execution price (always the maker's resting price)
    Quantity quantity{};       // Executed quantity
    Side     taker_side{};     // Aggressor side
};

static_assert(sizeof(Trade) <= 64, "Trade event should fit comfortably in a single 64-byte cache line");

} // namespace acme
