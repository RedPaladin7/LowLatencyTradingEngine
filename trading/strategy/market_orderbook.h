#pragma once 

#include "common/types.h"
#include "common/mem_pool.h"
#include "common/logging.h"

#include "market_order.h"
#include "exchange/market_data/market_update.h"

using namespace std;

namespace Trading {
    class TradingEngine;

    class MarketOrderBook final {
    public:
        MarketOrderBook(TickerId ticker_id, Logger *logger);

        ~MarketOrderBook();

        auto onMarketUpdate(const Exchange::MEMarketUpdate *market_update) noexcept -> void;

        // set trading engine 

        auto updateBBO(bool update_bid, bool update_ask) noexcept {
            if (update_bid) {
                if(bids_by_price_) {
                    bbo_.bid_price_ = bids_by_price_->price_;
                    bbo_.bid_qty_ = bids_by_price_->first_mkt_order_->qty_;
                    for(auto order = bids_by_price_->first_mkt_order_->next_order_;  order != bids_by_price_->first_mkt_order_; order = order->next_order_) {
                        bbo_.bid_qty_ += order->qty_;
                    }
                } else {
                    bbo_.bid_price_ = Price_INVALID;
                    bbo_.bid_qty_ = Qty_INVALID;
                }
            }

            if(update_ask) {
                if(asks_by_price_) {
                    bbo_.ask_price_ = asks_by_price_->price_;
                    bbo_.ask_qty_ = asks_by_price_->first_mkt_order_->qty_;
                    for(auto order = asks_by_price_->first_mkt_order_->next_order_;  order != asks_by_price_->first_mkt_order_; order = order->next_order_) {
                        bbo_.ask_qty_ += order->qty_;
                    }
                } else {
                    bbo_.ask_price_ = Price_INVALID;
                    bbo_.ask_qty_ = Qty_INVALID;
                }
            }
        }

        auto getBBO() const noexcept -> const BBO* {
            return &bbo_;
        }

        auto toString(bool detailed, bool validity_check) const -> string;

        MarketOrderBook() = delete;
        MarketOrderBook(const MarketOrderBook &) = delete;
        MarketOrderBook(const MarketOrderBook &&) = delete;
        MarketOrderBook &operator=(const MarketOrderBook &) = delete;
        MarketOrderBook &operator=(const MarketOrderBook &&) = delete;

    private:
        const TickerId ticker_id_;

        // TradeEngine *trade_engine_ = nullptr;

        OrderHashMap oid_to_order_;

        MemPool<MarketOrdersAtPrice> orders_at_price_pool_;
        MarketOrdersAtPrice *bids_by_price_ = nullptr;
        MarketOrdersAtPrice *asks_by_price_ = nullptr;

        OrdersAtPriceHashMap price_orders_at_price_;

        MemPool<MarketOrder> order_pool_;

        BBO bbo_;

        string time_str_;
        Logger *logger_ = nullptr;

    private: 
        auto priceToIndex(Price price) const noexcept {
            return (price % ME_MAX_PRICE_LEVELS);
        }

        auto getOrdersAtPrice(Price price) const noexcept -> MarketOrdersAtPrice* {
            return price_orders_at_price_.at(priceToIndex(price));
        }

        auto addOrdersAtPrice(MarketOrdersAtPrice *new_orders_at_price) noexcept {
        }
    };
}