#include "me_order_book.h"
#include "matcher/matching_engine.h"

namespace Exchange {
    MEOrderBook::MEOrderBook(TickerId ticker_id, Logger *logger, MatchingEngine *matching_engine)
    : ticker_id_(ticker_id), matching_engine_(matching_engine), logger_(logger), orders_at_price_pool_(ME_MAX_PRICE_LEVELS), order_pool_(ME_MAX_ORDER_IDS) {}

    MEOrderBook::~MEOrderBook() {
        logger_->log("%:% %() % OrderBook\n%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                toString(false, true));
        // remove pointer to matching engine, set entry points to nullptr
        matching_engine_ = nullptr;
        bids_by_price_ = asks_by_price_ = nullptr;
        for (auto &itr: cid_oid_to_order_) {
            itr.fill(nullptr);
        }
    }

    auto MEOrderBook::match(TickerId ticker_id, ClientId client_id, Side side, OrderId client_order_id, OrderId new_market_order_id, MEOrder *itr, Qty *leaves_qty) noexcept {
        // fetch info of the order that is going to be filled
        const auto order = itr;
        const auto order_qty = order->qty_; 
        const auto fill_qty = min(order_qty, *leaves_qty); 

        // update the actuall qty that will be filled for both clients
        *leaves_qty -= fill_qty; 
        order->qty_ -= fill_qty;

        // send reponse to client who filled the order
        client_response_ = {
            ClientResponseType::FILLED, 
            client_id, 
            ticker_id, 
            client_order_id, 
            new_market_order_id, 
            side, 
            itr->price_, 
            fill_qty, 
            *leaves_qty
        };
        matching_engine_->sendClientResponse(&client_response_);

        // send response to client whose order was filled
        client_response_ = {
            ClientResponseType::FILLED, 
            order->client_id_,
            ticker_id,
            order->client_order_id_,
            order->market_order_id_,
            order->side_,
            itr->price_,
            fill_qty,
            order->qty_
        };
        matching_engine_->sendClientResponse(&client_response_);

        // send market update
        market_update_ = {
            MarketUpdateType::TRADE,
            OrderId_INVALID,
            ticker_id,
            side,
            itr->price_,
            fill_qty,
            Priority_INVALID
        };
        matching_engine_->sendMarketUpdate(&market_update_);

        if(!order->qty_) {
            // the order was completely filled
            market_update_ = {
                MarketUpdateType::CANCEL,
                order->market_order_id_,
                ticker_id,
                order->side_,
                order->price_,
                order_qty,
                Priority_INVALID
            };
            matching_engine_->sendMarketUpdate(&market_update_);
            // remove the order from the orderbook
            removeOrder(order);
        } else {
            market_update_ = {
                MarketUpdateType::MODIFY,
                order->market_order_id_,
                ticker_id,
                order->side_,
                order->price_,
                order->qty_,
                order->priority_
            };
            matching_engine_->sendMarketUpdate(&market_update_);
        }
    }

    auto MEOrderBook::checkForMatch(ClientId client_id, OrderId client_order_id, TickerId ticker_id, Side side, Price price, Qty qty, Qty new_market_order_id) noexcept {
        auto leaves_qty = qty; 

        // keep filling the order as long as it is within the price range
        // return the remaining qty, so new order can be added in the orderbook if it is not zero
        if(side == Side::BUY){
            // iterate until you have leave qty and there are asks
            while(leaves_qty && asks_by_price_) {
                // get the best ask
                const auto ask_itr = asks_by_price_->first_me_order_;
                // if no match, return leave qty as it is
                if(LIKELY(price < ask_itr->price_)){
                    break;
                }
                match(ticker_id, client_id, side, client_order_id, new_market_order_id, ask_itr, &leaves_qty);
            }
        }
        if(side == Side::SELL) {
            // iterate until you have leave qty and there are bids
            while(leaves_qty && bids_by_price_) {
                // get the best bid
                const auto bid_itr = bids_by_price_->first_me_order_;
                // if not match, return leave qty as it is
                if(LIKELY(price > bid_itr->price_)) {
                    break;
                }
                match(ticker_id, client_id, side, client_order_id, new_market_order_id, bid_itr, &leaves_qty);
            }
        }
        return leaves_qty;
    }

    auto MEOrderBook::add(ClientId client_id, OrderId client_order_id, TickerId ticker_id, Side side, Price price, Qty qty) noexcept -> void {
        const auto new_market_order_id = generateNewMarketOrderId(); 
        // send immediate acknowledgment to client, that their order was accepted
        client_response_ = {
            ClientResponseType::ACCEPTED, 
            client_id,
            ticker_id,
            client_order_id,
            new_market_order_id,
            side,
            price, 0, 
            qty
        };
        matching_engine_->sendClientResponse(&client_response_);
        // check how much quantity is remaining after matching
        const auto leaves_qty = checkForMatch(client_id, client_order_id, ticker_id, side, price, qty, new_market_order_id);

        if(LIKELY(leaves_qty)) {
            // some quantity yet to be filled 
            const auto priority = getNextPriority(price);
            // create new passive order and add that to the orderbook 
            auto order = order_pool_.allocate(ticker_id, client_id, client_order_id, new_market_order_id, side, price, leaves_qty, priority, nullptr, nullptr);
            addOrder(order);

            // send market update that an order has been added 
            market_update_ = {
                MarketUpdateType::ADD,
                new_market_order_id,
                ticker_id,
                side,
                price,
                leaves_qty,
                priority
            };
            matching_engine_->sendMarketUpdate(&market_update_);
        }
    }

    auto MEOrderBook::cancel(ClientId client_id, OrderId order_id, TickerId ticker_id) noexcept -> void {
        // check for valid client id
        auto is_cancelable = (client_id < cid_oid_to_order_.size()); 
        MEOrder *exchange_order = nullptr; 
        if(LIKELY(is_cancelable)) {
            // check if the order is valid 
            auto &co_itr = cid_oid_to_order_.at(client_id);
            exchange_order = co_itr.at(order_id);
            is_cancelable = (exchange_order != nullptr);
        }
        if(UNLIKELY(!is_cancelable)) {
            // send message for cancel reject
            client_response_ = {
                ClientResponseType::CANCEL_REJECTED,
                client_id,
                ticker_id,
                order_id,
                OrderId_INVALID,
                Side::INVALID,
                Price_INVALID,
                Qty_INVALID,
                Qty_INVALID
            };
        } else {
            // successful cancel, send market update and client update 
            client_response_ = {
                ClientResponseType::CANCELED,
                client_id,
                ticker_id,
                order_id,
                exchange_order->market_order_id_,
                exchange_order->side_,
                exchange_order->price_,
                Qty_INVALID,
                exchange_order->qty_,
            };
            market_update_ = {
                MarketUpdateType::CANCEL,
                exchange_order->market_order_id_,
                ticker_id,
                exchange_order->side_,
                exchange_order->price_,
                0,
                exchange_order->priority_
            };
            removeOrder(exchange_order);
            matching_engine_->sendMarketUpdate(&market_update_);
        }
        matching_engine_->sendClientResponse(&client_response_);
    }

    auto MEOrderBook::toString(bool detailed, bool validity_check) const -> string {
        std::stringstream ss;
        std::string time_str;

        auto printer = [&](stringstream &ss, MEOrdersAtPrice *itr, Side side, Price &last_price, bool sanity_check) {
        char buf[4096];
        Qty qty = 0;
        size_t num_orders = 0;

        for (auto o_itr = itr->first_me_order_;; o_itr = o_itr->next_order_) {
            qty += o_itr->qty_;
            ++num_orders;
            if (o_itr->next_order_ == itr->first_me_order_)
            break;
        }
        sprintf(buf, " <px:%3s p:%3s n:%3s> %-3s @ %-5s(%-4s)",
                priceToString(itr->price_).c_str(), priceToString(itr->prev_entry_->price_).c_str(), priceToString(itr->next_entry_->price_).c_str(),
                priceToString(itr->price_).c_str(), qtyToString(qty).c_str(), to_string(num_orders).c_str());
        ss << buf;
        for (auto o_itr = itr->first_me_order_;; o_itr = o_itr->next_order_) {
            if (detailed) {
            sprintf(buf, "[oid:%s q:%s p:%s n:%s] ",
                    orderIdToString(o_itr->market_order_id_).c_str(), qtyToString(o_itr->qty_).c_str(),
                    orderIdToString(o_itr->prev_order_ ? o_itr->prev_order_->market_order_id_ : OrderId_INVALID).c_str(),
                    orderIdToString(o_itr->next_order_ ? o_itr->next_order_->market_order_id_ : OrderId_INVALID).c_str());
            ss << buf;
            }
            if (o_itr->next_order_ == itr->first_me_order_)
            break;
        }

        ss << endl;

        if (sanity_check) {
            if ((side == Side::SELL && last_price >= itr->price_) || (side == Side::BUY && last_price <= itr->price_)) {
            FATAL("Bids/Asks not sorted by ascending/descending prices last:" + priceToString(last_price) + " itr:" + itr->toString());
            }
            last_price = itr->price_;
        }
        };

        ss << "Ticker:" << tickerIdToString(ticker_id_) << endl;
        {
        auto ask_itr = asks_by_price_;
        auto last_ask_price = numeric_limits<Price>::min();
        for (size_t count = 0; ask_itr; ++count) {
            ss << "ASKS L:" << count << " => ";
            auto next_ask_itr = (ask_itr->next_entry_ == asks_by_price_ ? nullptr : ask_itr->next_entry_);
            printer(ss, ask_itr, Side::SELL, last_ask_price, validity_check);
            ask_itr = next_ask_itr;
        }
        }

        ss << endl << "                          X" << std::endl << std::endl;

        {
        auto bid_itr = bids_by_price_;
        auto last_bid_price = numeric_limits<Price>::max();
        for (size_t count = 0; bid_itr; ++count) {
            ss << "BIDS L:" << count << " => ";
            auto next_bid_itr = (bid_itr->next_entry_ == bids_by_price_ ? nullptr : bid_itr->next_entry_);
            printer(ss, bid_itr, Side::BUY, last_bid_price, validity_check);
            bid_itr = next_bid_itr;
        }
        }

        return ss.str();
  }
}