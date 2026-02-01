#pragma once 

#include "common/types.h"
#include "common/mem_pool.h"
#include "common/logging.h"
#include "order_server/client_response.h"
#include "market_data/market_update.h"

#include "me_order.h"
using namespace std;
using namespace Common;

namespace Exchange {
    class MatchingEngine; 

    class MEOrderBook final {
    public: 
        explicit MEOrderBook(TickerId ticker_id, Logger *logger, MatchingEngine *matching_engine);

        ~MEOrderBook();

        auto add (ClientId client_id, OrderId client_order_id, TickerId ticker_id, Side side, Price price, Qty qty) noexcept -> void;

        auto cancel(ClientId client_id, OrderId order_id, TickerId ticker_id) noexcept -> void;

        auto toString(bool detailed, bool validityCheck);

        MEOrderBook() = delete;
        MEOrderBook(const MEOrderBook &) = delete;
        MEOrderBook(const MEOrderBook &&) = delete;
        MEOrderBook &operator=(const MEOrderBook &) = delete;
        MEOrderBook &operator=(const MEOrderBook &&) = delete;

    private: 
        TickerId ticker_id_ = TickerId_INVALID;
        MatchingEngine *matching_engine_ = nullptr;
        ClientOrderHashMap cid_oid_to_order_;

        MemPool<MEOrdersAtPrice> orders_at_price_pool_;
        MEOrdersAtPrice *bids_by_price_ = nullptr;
        MEOrdersAtPrice *asks_by_price_ = nullptr;

        OrdersAtPriceHashMap price_orders_at_price_;

        MemPool<MEOrder> order_pool_;

        MEClientResponse client_response_;
        MEMarketUpdate market_update_;

        OrderId next_market_order_id_ = 1;

        string time_str_;
        Logger *logger_ = nullptr;
    };
}