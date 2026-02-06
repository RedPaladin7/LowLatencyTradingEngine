#pragma once 

#include "common/types.h"
#include "common/mem_pool.h"
#include "common/logging.h"
#include "order_server/client_response.h"
#include "market_data/market_update.h"

#include "me_order.h"
using namespace std;
using namespace Common;

// orderbook class for one trading instrument

namespace Exchange {
    class MatchingEngine; 

    class MEOrderBook final {
    public: 
        explicit MEOrderBook(TickerId ticker_id, Logger *logger, MatchingEngine *matching_engine);

        ~MEOrderBook();

        auto add (ClientId client_id, OrderId client_order_id, TickerId ticker_id, Side side, Price price, Qty qty) noexcept -> void;

        auto cancel(ClientId client_id, OrderId order_id, TickerId ticker_id) noexcept -> void;

        auto toString(bool detailed, bool validity_check) const -> string;

        MEOrderBook() = delete;
        MEOrderBook(const MEOrderBook &) = delete;
        MEOrderBook(const MEOrderBook &&) = delete;
        MEOrderBook &operator=(const MEOrderBook &) = delete;
        MEOrderBook &operator=(const MEOrderBook &&) = delete;

    private: 
        TickerId ticker_id_ = TickerId_INVALID;
        MatchingEngine *matching_engine_ = nullptr;
        // client -> all orders of that client hashmap
        ClientOrderHashMap cid_oid_to_order_;

        MemPool<MEOrdersAtPrice> orders_at_price_pool_;
        // points to highest price level for bids
        MEOrdersAtPrice *bids_by_price_ = nullptr;
        // points to the lowest price level for asks 
        MEOrdersAtPrice *asks_by_price_ = nullptr;

        // price -> orders at that price level hashmap
        OrdersAtPriceHashMap price_orders_at_price_;

        MemPool<MEOrder> order_pool_;

        MEClientResponse client_response_;
        MEMarketUpdate market_update_;

        OrderId next_market_order_id_ = 1;

        string time_str_;
        Logger *logger_ = nullptr;

    private: 
        // unique for all the orders (strict increment)
        auto generateNewMarketOrderId() noexcept -> OrderId {
            return next_market_order_id_++;
        }

        // converts the price to index to be used in hashmap
        auto priceToIndex(Price price) const noexcept {
            return (price % ME_MAX_PRICE_LEVELS);
        }

        auto getOrdersAtPrice(Price price) const noexcept -> MEOrdersAtPrice * {
            return price_orders_at_price_.at(priceToIndex(price));
        }

        // add a price level (the order that we want to add is the first one at that price level)
        auto addOrdersAtPrice(MEOrdersAtPrice *new_orders_at_price) noexcept {
            // assign new order to that price level 
            price_orders_at_price_.at(priceToIndex(new_orders_at_price->price_)) = new_orders_at_price;
            // get the highest bid price or lowest ask price (entry point)
            const auto best_orders_by_price = (new_orders_at_price->side_ == Side::BUY ? bids_by_price_ : asks_by_price_);

            if(UNLIKELY(!best_orders_by_price)){
                // if no best order -> new order is the first one in the orderbook
                // assign that as entry point to bids / asks
                (new_orders_at_price->side_ == Side::BUY ? bids_by_price_ : asks_by_price_) = new_orders_at_price;
                // point the price level to itself (circular doubly linked list)
                new_orders_at_price->prev_entry_ = new_orders_at_price->next_entry_ = new_orders_at_price;
            } else {
                // traverse until you find the correct price level 
                auto target = best_orders_by_price;
                // condition: lower bid price or higher ask price 
                bool add_after = (
                    (new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ > target->price_) || 
                    (new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ < target->price_)
                );
                // early check because new order prices are usually close to the best bids or asks 
                if(add_after) {
                    target = target->next_entry_;
                    add_after = (
                        (new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ > target->price_) || 
                        (new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ < target->price_)
                    );
                }
                while(add_after && target != best_orders_by_price) {
                    add_after = (
                        (new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ > target->price_) || 
                        (new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ < target->price_)
                    );
                    if(add_after) {
                        target = target->next_entry_;
                    }
                }
                // actually add the order at the right place
                if(add_after){
                    // we have reached the end of all orders at that price level
                    if(target == best_orders_by_price){
                        target = best_orders_by_price->prev_entry_;
                    }
                    new_orders_at_price->prev_entry_ = target;
                    target->next_entry_->prev_entry_ = new_orders_at_price;
                    new_orders_at_price->next_entry_ = target->next_entry_;
                    target->next_entry_ = new_orders_at_price;
                } else {
                    new_orders_at_price->prev_entry_ = target->prev_entry_;
                    new_orders_at_price->next_entry_ = target;
                    target->prev_entry_->next_entry_ = new_orders_at_price;
                    target->prev_entry_= new_orders_at_price;

                    if (
                        (new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ > target->price_) || 
                        (new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ < target->price_)
                    ){
                        // new order is the best bid or ask
                        target->next_entry_ = (target->next_entry_ == best_orders_by_price ? new_orders_at_price : target->next_entry_);
                        (new_orders_at_price->side_ == Side::BUY ? bids_by_price_ : bids_by_price_) = new_orders_at_price;
                    }
                }
            }
        }

        // remove a price level (the order we want to remove is the only one at that price level)
        auto removeOrdersAtPrice(Side side, Price price) noexcept {
            // get entry point
            const auto best_orders_by_price = (side == Side::BUY ? bids_by_price_ : asks_by_price_);
            // get all orders at given price 
            auto orders_at_price = getOrdersAtPrice(price);

            if(UNLIKELY(orders_at_price->next_entry_ == orders_at_price)) {
                // if it is the only price level in the order book 
                (side == Side::BUY ? bids_by_price_ : asks_by_price_) = nullptr;
            } else {
                // removing price level from linked list 
                orders_at_price->prev_entry_->next_entry_ = orders_at_price->next_entry_;
                orders_at_price->next_entry_->prev_entry_ = orders_at_price->prev_entry_;

                if(orders_at_price == best_orders_by_price) {
                    // if the price to remove was the best order, set next entry as best order
                    (side == Side::BUY ? bids_by_price_ : asks_by_price_) = orders_at_price->next_entry_;
                }
                // remove next and prev pointers for price level to remove 
                orders_at_price->prev_entry_ = orders_at_price->next_entry_ = nullptr;
            }
            // deallocate space for that price level 
            price_orders_at_price_ .at(priceToIndex(price)) = nullptr;
            orders_at_price_pool_.deallocate(orders_at_price);
        }

        auto getNextPriority(Price price) noexcept {
            const auto orders_at_price = getOrdersAtPrice(price);
            if(!orders_at_price){
                return 1lu;
            }
            // get the priority of the last order at that price level + 1
            return orders_at_price->first_me_order_->prev_order_->priority_ + 1;
        }

        auto match(TickerId ticker_id, ClientId client_id, Side side, OrderId client_order_id, OrderId new_market_order_id, MEOrder *bid_itr, Qty *leaves_qty) noexcept;

        auto checkForMatch(ClientId client_id, OrderId client_order_id, TickerId ticker_id, Side side, Price price, Qty qty, Qty new_market_order_id) noexcept;

        // remove a single order
        auto removeOrder(MEOrder *order) noexcept {
            auto orders_at_price = getOrdersAtPrice(order->price_);

            if(order->prev_order_ == order){
                // if it is the only order at that price level, remove the whole price level 
                removeOrdersAtPrice(order->side_, order->price_);
            } else {
                // remove the order from orders at that price level 
                const auto order_before = order->prev_order_;
                const auto order_after = order->next_order_; 
                order_before->next_order_ = order_after;
                order_after->prev_order_ = order_before;

                if(orders_at_price->first_me_order_ == order) {
                    // if it was the first order at that price level, 
                    // set the first memeber as next order 
                    orders_at_price->first_me_order_ = order_after;
                }
                // clean up previous and next order pointers
                order->prev_order_ = order->next_order_ = nullptr;
            }

            // remove from client -> orders hashmap AND deallocate memory from order pool
            cid_oid_to_order_.at(order->client_id_).at(order->client_order_id_) = nullptr;
            order_pool_.deallocate(order);
        }
        
        // add a single order
        auto addOrder(MEOrder *order) noexcept {
            const auto orders_at_price = getOrdersAtPrice(order->price_);

            if(!orders_at_price){
                // point the next and prev pointers to itself (it is the only order at that price level)
                order->next_order_ = order->prev_order_ = order;
                // alocate space in memory pool
                auto new_orders_at_price = orders_at_price_pool_.allocate(order->side_, order->price_, order, nullptr, nullptr);
                // create a new price level 
                addOrdersAtPrice(new_orders_at_price);
            } else {
                auto first_order = (orders_at_price ? orders_at_price->first_me_order_ : nullptr);

                // add order as the last order at that price level 
                first_order->prev_order_->next_order_ = order;
                order->prev_order_ = first_order->prev_order_;
                order->next_order_ = first_order;
                first_order->prev_order_ = order;
            }
            // create entry in client to orders hashmap 
            cid_oid_to_order_.at(order->client_id_).at(order->client_order_id_) = order;
        }
    };

    // ticker -> orderbook hashmap
    typedef array<MEOrderBook *, ME_MAX_TICKERS> OrderBookHashMap;
}