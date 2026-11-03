#include "acme/order_book.hpp"

#include <iomanip>
#include <iostream>

namespace acme {

OrderBook::OrderBook(std::size_t max_resting_orders, std::size_t max_price_levels)
    : pool_(max_resting_orders),
      order_index_(max_resting_orders),
      max_price_levels_(max_price_levels) {
    // Pre-allocate contiguous price level vectors so hot-path insertions never reallocate
    bids_.reserve(max_price_levels_);
    asks_.reserve(max_price_levels_);
}

Quantity OrderBook::add_order(OrderId id, Side side, Price price, Quantity quantity,
                              OrderType type, std::vector<Trade>* out_trades) {
    return add_order(id, side, price, quantity, type, [out_trades](const Trade& trade) noexcept {
        if (out_trades != nullptr) {
            out_trades->push_back(trade);
        }
    });
}

PriceLevel* OrderBook::find_or_create_level(Side side, Price price) noexcept {
    std::vector<PriceLevel>& levels = (side == Side::Buy) ? bids_ : asks_;

    // Search from `.end()` backward because resting orders almost always arrive
    // near the best bid/ask at the top of the book (`.back()`).
    auto it = levels.end();
    while (it != levels.begin()) {
        auto prev = it - 1;
        if (prev->price == price) {
            return &(*prev);
        }
        // bids_ sorted ascending (best highest bid at back); asks_ sorted descending (best lowest ask at back)
        const bool goes_after_prev = (side == Side::Buy)
            ? (price > prev->price)
            : (price < prev->price);
        if (goes_after_prev) {
            break;
        }
        it = prev;
    }

    if (levels.size() >= max_price_levels_) [[unlikely]] {
        return nullptr; // Price level capacity reached
    }

    auto inserted_it = levels.insert(it, PriceLevel{.price = price});
    return &(*inserted_it);
}

PriceLevel* OrderBook::find_level(Side side, Price price) noexcept {
    std::vector<PriceLevel>& levels = (side == Side::Buy) ? bids_ : asks_;
    for (auto it = levels.rbegin(); it != levels.rend(); ++it) {
        if (it->price == price) {
            return &(*it);
        }
        const bool passed_target = (side == Side::Buy)
            ? (it->price < price)
            : (it->price > price);
        if (passed_target) {
            break;
        }
    }
    return nullptr;
}

void OrderBook::erase_level(Side side, PriceLevel* level) noexcept {
    std::vector<PriceLevel>& levels = (side == Side::Buy) ? bids_ : asks_;
    assert(level >= levels.data() && level < levels.data() + levels.size());
    const auto idx = static_cast<std::size_t>(level - levels.data());
    levels.erase(levels.begin() + static_cast<std::ptrdiff_t>(idx));
}

bool OrderBook::insert_resting_order(OrderId id, Side side, Price price, Quantity quantity) noexcept {
    OrderNode* node = pool_.allocate(OrderNode{
        .id       = id,
        .price    = price,
        .quantity = quantity,
        .side     = side,
        .prev     = nullptr,
        .next     = nullptr
    });

    if (node == nullptr) [[unlikely]] {
        return false;
    }

    PriceLevel* level = find_or_create_level(side, price);
    if (level == nullptr) [[unlikely]] {
        pool_.deallocate(node);
        return false;
    }

    if (!order_index_.insert(id, node)) [[unlikely]] {
        if (level->empty()) {
            erase_level(side, level);
        }
        pool_.deallocate(node);
        return false;
    }

    level->append(node);
    return true;
}

bool OrderBook::cancel_order(OrderId id) noexcept {
    OrderNode** found = order_index_.find(id);
    if (found == nullptr) {
        return false;
    }

    OrderNode* const node = *found;
    const Price price     = node->price;
    const Side side       = node->side;

    PriceLevel* level = find_level(side, price);
    if (level != nullptr) {
        level->unlink(node);
        if (level->empty()) {
            erase_level(side, level);
        }
    }

    order_index_.erase(id);
    pool_.deallocate(node);
    return true;
}

bool OrderBook::reduce_order(OrderId id, Quantity new_quantity) noexcept {
    if (new_quantity == 0) {
        return cancel_order(id);
    }

    OrderNode** found = order_index_.find(id);
    if (found == nullptr) {
        return false;
    }

    OrderNode* const node = *found;
    if (new_quantity >= node->quantity) {
        return false; // Size increase loses FIFO priority on exchanges; requires cancel+replace
    }

    const Quantity delta = node->quantity - new_quantity;
    node->quantity       = new_quantity;

    PriceLevel* level = find_level(node->side, node->price);
    if (level != nullptr) {
        level->total_quantity -= delta;
    }
    return true;
}

BookTop OrderBook::top_of_book() const noexcept {
    BookTop top{};
    if (!bids_.empty()) {
        const PriceLevel& best_bid = bids_.back();
        top.best_bid               = best_bid.price;
        top.bid_quantity           = best_bid.total_quantity;
    }
    if (!asks_.empty()) {
        const PriceLevel& best_ask = asks_.back();
        top.best_ask               = best_ask.price;
        top.ask_quantity           = best_ask.total_quantity;
    }
    return top;
}

void OrderBook::print_depth(std::size_t max_levels) const {
    std::cout << "\n+----------------------------------------------------+\n";
    std::cout << "|             ACME LIMIT ORDER BOOK (L2)             |\n";
    std::cout << "+----------------+-------------------+---------------+\n";
    std::cout << "|      SIDE      |       PRICE       |  TOTAL QTY    |\n";
    std::cout << "+----------------+-------------------+---------------+\n";

    // asks_ has lowest (best) ask at .back(); print highest ask among top N down to best ask
    const std::size_t ask_count = std::min(max_levels, asks_.size());
    for (std::size_t i = ask_count; i > 0; --i) {
        const PriceLevel& level = asks_[asks_.size() - i];
        std::cout << "|  ASK (Sell)    | $" << std::setw(16) << std::fixed << std::setprecision(4)
                  << to_human_price(level.price) << " | " << std::setw(8) << level.total_quantity
                  << " (" << level.order_count << ") |\n";
    }

    std::cout << "+----------------+-------------------+---------------+\n";

    // bids_ has highest (best) bid at .back(); print from best bid downward
    const std::size_t bid_count = std::min(max_levels, bids_.size());
    for (std::size_t i = 0; i < bid_count; ++i) {
        const PriceLevel& level = bids_[bids_.size() - 1U - i];
        std::cout << "|  BID (Buy)     | $" << std::setw(16) << std::fixed << std::setprecision(4)
                  << to_human_price(level.price) << " | " << std::setw(8) << level.total_quantity
                  << " (" << level.order_count << ") |\n";
    }
    std::cout << "+----------------+-------------------+---------------+\n\n";
}

} // namespace acme
