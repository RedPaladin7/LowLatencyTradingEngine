#pragma once 

#include <array>
#include <sstream>
#include "common/types.h"

using namespace std;
using namespace Common; 

// struct to define single order in orderbook
// hashmap implented using array -> memory address used as keys (converted using index function)\
// assign all space at once -> maybe sparse
// hence we have pointers to prev and next entries 

namespace Exchange {
    struct MEOrder {
        TickerId ticker_id_ = TickerId_INVALID;
        ClientId client_id_ = ClientId_INVALID;
        // client order id to identify order on client side, may not be for the market side 
        // market order id to identify order on market side, completely unique, assigned by matcher
        OrderId client_order_id_ = OrderId_INVALID;
        OrderId market_order_id_ = OrderId_INVALID;
        Side side_ = Side::INVALID;
        Price price_ = Price_INVALID;
        Qty qty_ = Qty_INVALID;
        Priority priority_ = Priority_INVALID;

        // circular doubly linked list
        // points to prev and next order at the same price level 
        MEOrder *prev_order_ = nullptr;
        MEOrder *next_order_ = nullptr;

        MEOrder() = default; 

        MEOrder(TickerId ticker_id, ClientId client_id, OrderId client_order_id, OrderId market_order_id, Side side, Price price, Qty qty, Priority priority, MEOrder *prev_order, MEOrder *next_order) noexcept
        : ticker_id_(ticker_id), client_id_(client_id), client_order_id_(client_order_id), market_order_id_(market_order_id), 
        side_(side), price_(price), qty_(qty), priority_(priority), prev_order_(prev_order), next_order_(next_order) {}

        auto toString() const -> string;
    };

    typedef array<MEOrder *, ME_MAX_ORDER_IDS> OrderHashMap;
    // 2d array, client -> array of order for that client 
    typedef array<OrderHashMap, ME_MAX_NUM_CLIENTS> ClientOrderHashMap;

    // represent orders at the same price level 
    struct MEOrdersAtPrice {
        Side side_ = Side::INVALID;
        Price price_ = Price_INVALID;

        // entry point to the best bid or ask at that price level 
        MEOrder *first_me_order_ = nullptr; 
        // also circular doubly linked list 
        // has pointers to next and prev price levels 
        MEOrdersAtPrice *prev_entry_ = nullptr;
        MEOrdersAtPrice *next_entry_ = nullptr;

        MEOrdersAtPrice() = default; 

        MEOrdersAtPrice(Side side, Price price, MEOrder *first_me_order, MEOrdersAtPrice *prev_entry, MEOrdersAtPrice *next_entry)
         : side_(side), price_(price), first_me_order_(first_me_order), prev_entry_(prev_entry), next_entry_(next_entry) {}

        auto toString() const {
            stringstream ss;
            ss  << "MEOrdersAtPrice["
                << "side:" << sideToString(side_) << " "
                << "price:" << priceToString(price_) << " "
                << "first_me_order:" << (first_me_order_ ? first_me_order_->toString() : "null") << " "
                << "prev:" << priceToString(prev_entry_ ? prev_entry_->price_ : Price_INVALID) << " "
                << "next:" << priceToString(next_entry_ ? next_entry_->price_ : Price_INVALID) << "]";
            return ss.str();
        }
    };

    // price to orders at that price
    typedef array<MEOrdersAtPrice *, ME_MAX_PRICE_LEVELS> OrdersAtPriceHashMap;
}