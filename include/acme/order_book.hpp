#pragma once

#include "acme/flat_hash_map.hpp"
#include "acme/object_pool.hpp"
#include "acme/types.hpp"

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace acme {

// ============================================================================
// LESSON 6: Intrusive Doubly-Linked List for O(1) Order Cancellation
// ============================================================================
// In an exchange order book, ~90-98% of orders placed by market makers are
// canceled before they ever trade! Therefore, `cancel_order(OrderId)` MUST be O(1).
//
// By embedding `prev` and `next` pointers directly inside `OrderNode`
// (an "intrusive" linked list) and storing `OrderId -> OrderNode*` in a
// pre-allocated `FlatHashMap`, we can unlink any order from the middle of a
// price level in O(1) with zero memory allocations.
struct OrderNode {
    OrderId    id{};
    Price      price{};
    Quantity   quantity{};
    Side       side{Side::Buy};
    OrderNode* prev{nullptr};
    OrderNode* next{nullptr};
};

// Represents all resting orders at a single price point, ordered by arrival time (FIFO)
struct PriceLevel {
    Price       price{};
    Quantity    total_quantity{0};
    std::size_t order_count{0};
    OrderNode*  head{nullptr}; // Oldest order (highest time priority, matched first)
    OrderNode*  tail{nullptr}; // Newest order (appended at the back)

    void append(OrderNode* node) noexcept {
        assert(node != nullptr);
        node->prev = tail;
        node->next = nullptr;
        if (tail != nullptr) {
            tail->next = node;
        } else {
            head = node;
        }
        tail = node;
        total_quantity += node->quantity;
        ++order_count;
    }

    void unlink(OrderNode* node) noexcept {
        assert(node != nullptr);
        if (node->prev != nullptr) {
            node->prev->next = node->next;
        } else {
            head = node->next;
        }
        if (node->next != nullptr) {
            node->next->prev = node->prev;
        } else {
            tail = node->prev;
        }
        node->prev = nullptr;
        node->next = nullptr;
        total_quantity -= node->quantity;
        --order_count;
    }

    [[nodiscard]] bool empty() const noexcept {
        return head == nullptr;
    }
};

// ============================================================================
// LESSON 7: C++20 Concepts for Zero-Cost Compile-Time Polymorphism
// ============================================================================
// Instead of using `std::function<void(const Trade&)>` (which can heap-allocate
// and incurs an indirect pointer call on every trade), we use a C++20 `concept`
// so the compiler completely inlines the callback at the call site!
template <typename F>
concept TradeListener = std::invocable<F, const Trade&>;

struct BookTop {
    std::optional<Price> best_bid{};
    Quantity             bid_quantity{0};
    std::optional<Price> best_ask{};
    Quantity             ask_quantity{0};
};

// ============================================================================
// BEST PRACTICE: Contiguous Flat Price Ladder (Best Price at `.back()`)
// ============================================================================
// Standard `std::map` is a red-black tree that allocates every node on the heap
// and scatters price levels across memory.
//
// Instead, we store `bids_` and `asks_` in pre-reserved contiguous `std::vector`s
// sorted so the BEST price is always at `.back()`:
//   - `bids_`: sorted ascending  -> `bids_.back()` is the Highest (Best) Bid.
//   - `asks_`: sorted descending -> `asks_.back()` is the Lowest (Best) Ask.
// When a best price level is completely filled during matching, removing it is
// a single `pop_back()` instruction in O(1) time with zero element shifting!
class OrderBook {
public:
    explicit OrderBook(std::size_t max_resting_orders = 100'000,
                       std::size_t max_price_levels   = 4'096);

    OrderBook(const OrderBook&)            = delete;
    OrderBook& operator=(const OrderBook&) = delete;
    OrderBook(OrderBook&&) noexcept        = default;
    OrderBook& operator=(OrderBook&&) noexcept = default;
    ~OrderBook()                           = default;

    // Add a new order and match immediately if it crosses the spread.
    // Returns remaining resting quantity (0 if fully filled, rejected, or IOC/Market).
    template <TradeListener OnTrade>
    Quantity add_order(OrderId id, Side side, Price price, Quantity quantity,
                       OrderType type, OnTrade&& on_trade) noexcept;

    // Convenience overload when caller wants to collect trades in a vector (for testing)
    Quantity add_order(OrderId id, Side side, Price price, Quantity quantity,
                       OrderType type = OrderType::Limit,
                       std::vector<Trade>* out_trades = nullptr);

    // Cancel an existing resting order in O(1) lookup + unlink
    bool cancel_order(OrderId id) noexcept;

    // Modify quantity downward in-place without losing FIFO time priority
    bool reduce_order(OrderId id, Quantity new_quantity) noexcept;

    // Query top-of-book (BBO: Best Bid and Offer) in O(1) time
    [[nodiscard]] BookTop top_of_book() const noexcept;

    [[nodiscard]] std::size_t resting_order_count() const noexcept {
        return order_index_.size();
    }

    // Print human-readable L2 Market Depth Ladder
    void print_depth(std::size_t max_levels = 5) const;

private:
    template <TradeListener OnTrade>
    void match_against_side(OrderId taker_id, Side taker_side, Price limit_price,
                            Quantity& remaining_qty, std::vector<PriceLevel>& opposite_levels,
                            OnTrade&& on_trade) noexcept;

    bool insert_resting_order(OrderId id, Side side, Price price, Quantity quantity) noexcept;

    [[nodiscard]] PriceLevel* find_or_create_level(Side side, Price price) noexcept;
    [[nodiscard]] PriceLevel* find_level(Side side, Price price) noexcept;
    void erase_level(Side side, PriceLevel* level) noexcept;

    // Pre-allocated memory pool for OrderNode objects
    ObjectPool<OrderNode> pool_;

    // Pre-allocated open-addressing flat hash map: OrderId -> OrderNode*
    FlatHashMap<OrderId, OrderNode*> order_index_;

    // Contiguous price level ladders (Best Bid = bids_.back(), Best Ask = asks_.back())
    std::vector<PriceLevel> bids_;
    std::vector<PriceLevel> asks_;
    std::size_t             max_price_levels_;
};

// ============================================================================
// Template Implementation (in header so compiler can inline across translation units)
// ============================================================================
template <TradeListener OnTrade>
Quantity OrderBook::add_order(OrderId id, Side side, Price price, Quantity quantity,
                              OrderType type, OnTrade&& on_trade) noexcept {
    // Validate input parameters on entry
    if (quantity == 0) [[unlikely]] {
        return 0;
    }
    if (type != OrderType::Market && price <= 0) [[unlikely]] {
        return 0;
    }
    if (order_index_.find(id) != nullptr) [[unlikely]] {
        return 0; // Reject duplicate OrderId
    }

    Quantity remaining_qty = quantity;

    if (side == Side::Buy) {
        const Price effective_price = (type == OrderType::Market)
            ? std::numeric_limits<Price>::max()
            : price;
        match_against_side(id, side, effective_price, remaining_qty, asks_, on_trade);
    } else {
        const Price effective_price = (type == OrderType::Market)
            ? std::numeric_limits<Price>::min()
            : price;
        match_against_side(id, side, effective_price, remaining_qty, bids_, on_trade);
    }

    // Only Limit orders with remaining quantity rest in the book
    if (remaining_qty > 0 && type == OrderType::Limit) {
        if (insert_resting_order(id, side, price, remaining_qty)) {
            return remaining_qty;
        }
    }

    return 0;
}

template <TradeListener OnTrade>
void OrderBook::match_against_side(OrderId taker_id, Side taker_side, Price limit_price,
                                   Quantity& remaining_qty, std::vector<PriceLevel>& opposite_levels,
                                   OnTrade&& on_trade) noexcept {
    // Because the best price is always stored at `.back()`, we match against
    // `opposite_levels.back()` and simply `pop_back()` when a level is exhausted!
    while (!opposite_levels.empty() && remaining_qty > 0) {
        PriceLevel& best_level          = opposite_levels.back();
        const Price best_opposite_price = best_level.price;

        const bool crosses = (taker_side == Side::Buy)
            ? (limit_price >= best_opposite_price)
            : (limit_price <= best_opposite_price);

        if (!crosses) {
            break; // Incoming order does not cross the spread
        }

        OrderNode* maker = best_level.head;

        // Match in strict FIFO time priority within this price level
        while (maker != nullptr && remaining_qty > 0) {
            OrderNode* const next_maker = maker->next;
            const Quantity fill_qty     = std::min(remaining_qty, maker->quantity);

            on_trade(Trade{
                .maker_order_id = maker->id,
                .taker_order_id = taker_id,
                .price          = best_opposite_price,
                .quantity       = fill_qty,
                .taker_side     = taker_side
            });

            remaining_qty -= fill_qty;

            if (fill_qty == maker->quantity) {
                // Maker order completely filled: unlink(maker) subtracts maker->quantity from best_level.total_quantity
                best_level.unlink(maker);
                order_index_.erase(maker->id);
                pool_.deallocate(maker);
            } else {
                // Partial fill of resting maker order
                maker->quantity           -= fill_qty;
                best_level.total_quantity -= fill_qty;
            }

            maker = next_maker;
        }

        if (best_level.empty()) {
            opposite_levels.pop_back(); // O(1) removal of exhausted best price level!
        }
    }
}

} // namespace acme
