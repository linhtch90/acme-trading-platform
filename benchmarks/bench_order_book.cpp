#include "acme/order_book.hpp"
#include "acme/types.hpp"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <vector>

#if defined(__x86_64__) || defined(_M_X64)
#include <x86intrin.h>
#define ACME_HAS_RDTSC 1
#else
#define ACME_HAS_RDTSC 0
#endif

using namespace acme;

namespace {

[[nodiscard]] inline std::uint64_t read_tsc() noexcept {
#if ACME_HAS_RDTSC
    return __rdtsc();
#else
    return static_cast<std::uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
#endif
}

} // namespace

// ============================================================================
// LESSON 8: Benchmarking Tail Latency with CPU TSC (`__rdtsc`) vs Clock Overhead
// ============================================================================
// Calling `std::chrono::steady_clock::now()` on every single order can cost
// hundreds of nanoseconds in virtualized environments—more than the order book
// operation itself!
//
// HFT engineers benchmark in two ways:
//   1. Uninstrumented batch throughput (measures true ns/op without timer overhead).
//   2. Per-order cycle distribution using the x86 hardware Time Stamp Counter (`__rdtsc`).
int main() {
    constexpr std::size_t NUM_ORDERS = 200'000;

    // ------------------------------------------------------------------------
    // Pass 1: Uninstrumented Pure Throughput Benchmark
    // ------------------------------------------------------------------------
    std::uint64_t total_trades = 0;
    double elapsed_sec = 0.0;
    const std::size_t total_ops = NUM_ORDERS + (NUM_ORDERS / 3U);

    {
        OrderBook book(NUM_ORDERS + 10'000, 4'096);
        auto on_trade = [&total_trades](const Trade&) noexcept {
            ++total_trades;
        };

        const auto t_start = std::chrono::steady_clock::now();
        for (std::size_t i = 1; i <= NUM_ORDERS; ++i) {
            const Side side    = (i % 2U == 0U) ? Side::Buy : Side::Sell;
            const Price offset = static_cast<Price>((i * 17U) % 40U) - 20;
            const Price price  = to_ticks(100.00) + offset * 50;
            const Quantity qty = static_cast<Quantity>(10U + (i % 90U));

            book.add_order(static_cast<OrderId>(i), side, price, qty, OrderType::Limit, on_trade);

            if (i % 3U == 0U && i > 2U) {
                book.cancel_order(static_cast<OrderId>(i - 2U));
            }
        }
        const auto t_end = std::chrono::steady_clock::now();
        elapsed_sec = std::chrono::duration<double>(t_end - t_start).count();
    }

    // ------------------------------------------------------------------------
    // Pass 2: Per-Operation Tail Latency Distribution using Hardware `__rdtsc()`
    // ------------------------------------------------------------------------
    std::vector<std::uint64_t> add_cycles;
    std::vector<std::uint64_t> cancel_cycles;
    add_cycles.reserve(NUM_ORDERS);
    cancel_cycles.reserve(NUM_ORDERS / 3U);

    {
        OrderBook book(NUM_ORDERS + 10'000, 4'096);
        auto noop_trade = [](const Trade&) noexcept {};

        for (std::size_t i = 1; i <= NUM_ORDERS; ++i) {
            const Side side    = (i % 2U == 0U) ? Side::Buy : Side::Sell;
            const Price offset = static_cast<Price>((i * 17U) % 40U) - 20;
            const Price price  = to_ticks(100.00) + offset * 50;
            const Quantity qty = static_cast<Quantity>(10U + (i % 90U));

            const std::uint64_t c0 = read_tsc();
            book.add_order(static_cast<OrderId>(i), side, price, qty, OrderType::Limit, noop_trade);
            const std::uint64_t c1 = read_tsc();
            add_cycles.push_back(c1 - c0);

            if (i % 3U == 0U && i > 2U) {
                const std::uint64_t k0 = read_tsc();
                book.cancel_order(static_cast<OrderId>(i - 2U));
                const std::uint64_t k1 = read_tsc();
                cancel_cycles.push_back(k1 - k0);
            }
        }
    }

    std::sort(add_cycles.begin(), add_cycles.end());
    std::sort(cancel_cycles.begin(), cancel_cycles.end());

    auto percentile = [](const std::vector<std::uint64_t>& v, double p) {
        const std::size_t idx = static_cast<std::size_t>(p * static_cast<double>(v.size() - 1U));
        return v[idx];
    };

    const double throughput_mops = static_cast<double>(total_ops) / elapsed_sec / 1e6;
    const double mean_ns_per_op  = (elapsed_sec * 1e9) / static_cast<double>(total_ops);

    std::cout << "========================================================\n";
    std::cout << "  ACME Matching Engine Latency & Throughput Benchmark\n";
    std::cout << "========================================================\n";
    std::cout << "Total Operations   : " << total_ops << " (" << NUM_ORDERS << " adds, "
              << cancel_cycles.size() << " cancels)\n";
    std::cout << "Trades Executed    : " << total_trades << "\n";
    std::cout << "Pure Throughput    : " << std::fixed << std::setprecision(2)
              << throughput_mops << " million ops/sec ("
              << std::setprecision(1) << mean_ns_per_op << " ns/op average)\n\n";

    std::cout << "Add/Match Order Latency (CPU Cycles via __rdtsc):\n";
    std::cout << "  p50   (Median) : " << std::setw(6) << percentile(add_cycles, 0.50) << " cycles\n";
    std::cout << "  p90            : " << std::setw(6) << percentile(add_cycles, 0.90) << " cycles\n";
    std::cout << "  p99            : " << std::setw(6) << percentile(add_cycles, 0.99) << " cycles\n";
    std::cout << "  p99.9          : " << std::setw(6) << percentile(add_cycles, 0.999) << " cycles\n\n";

    std::cout << "Cancel Order Latency (CPU Cycles via __rdtsc):\n";
    std::cout << "  p50   (Median) : " << std::setw(6) << percentile(cancel_cycles, 0.50) << " cycles\n";
    std::cout << "  p90            : " << std::setw(6) << percentile(cancel_cycles, 0.90) << " cycles\n";
    std::cout << "  p99            : " << std::setw(6) << percentile(cancel_cycles, 0.99) << " cycles\n";
    std::cout << "  p99.9          : " << std::setw(6) << percentile(cancel_cycles, 0.999) << " cycles\n";
    std::cout << "========================================================\n";

    return 0;
}
