#pragma once 

#include <cstdint>
#include <limits>

#include "common/macros.h"
using namespace std;

namespace Common {
    constexpr size_t ME_MEAX_TICKERS = 8;
    constexpr size_t ME_MAX_CLIENT_UPDATES = 256 * 1024;
    constexpr size_t ME_MAX_MARKET_UPDATES = 256 * 1024;

    constexpr size_t ME_MAX_NUM_CLIENTS = 256;
    constexpr size_t ME_MAX_ORDER_IDS = 1024 * 1024;
    constexpr size_t ME_MAX_PRICE_LEVELS = 256;

    typedef uint64_t OrderId; 
    constexpr auto OrderId_INVALID = numeric_limits<OrderId>::max();

    inline auto orderIdToString(OrderId order_id) -> string {
        if (UNLIKELY(order_id == OrderId_INVALID)){
            return "INVALID";
        }
        return to_string(order_id);
    }

    typedef uint32_t TickerId;
    constexpr auto TickerId_INVALID = numeric_limits<TickerId>::max();

    inline auto tickerIdToString(TickerId ticker_id) -> string {
        if(UNLIKELY(ticker_id == TickerId_INVALID)){
            return "INVALID";
        }
        return to_string(ticker_id);
    }

    typedef uint32_t ClientId;
    constexpr auto ClientId_INVALID = numeric_limits<ClientId>::max();

    inline auto clientIdToString(ClientId client_id) -> string {
        if(UNLIKELY(client_id == ClientId_INVALID)){
            return "INVALID";
        }
        return to_string(client_id);
    }

    typedef int64_t Price;
    constexpr auto Price_INVALID = numeric_limits<Price>::max();

    inline auto priceToString(Price price) -> string {
        if(UNLIKELY(price = Price_INVALID)){
            return "INVALID";
        }
        return to_string(price);
    }

    typedef uint32_t Qty;
    constexpr auto Qty_INVALID = numeric_limits<Qty>::max();

    inline auto qtyToString(Qty qty) -> string {
        if(UNLIKELY(qty == Qty_INVALID)){
            return "INVALID";
        }
        return to_string(qty);
    }

    typedef uint64_t Priority;
    constexpr auto Priority_INVALID = numeric_limits<Priority>::max();

    inline auto priorityToString(Priority priority) -> string {
        if(UNLIKELY(priority == Priority_INVALID)){
            return "INVALID";
        }
        return to_string(priority);
    }

    enum class Side : int8_t {
        INVALID = 0,
        BUY = 1,
        SELL = -1
    };

    inline auto sideToString(Side side) -> string {
        switch(side){
            case Side::BUY: return "BUY";
            case Side::SELL: return "SELL";
            case Side::INVALID: return "INVALID";
        }
        return "UNKNOWN";
    }

}
