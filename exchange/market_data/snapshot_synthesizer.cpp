#include "snapshot_synthesizer.h"

// sends an updated copy of the current state of the order book
// used to retrieve any data lost if packets were dropped during the udp connection
namespace Exchange {
    SnapshotSynthesizer::SnapshotSynthesizer(MDPMarketUpdateLFQueue *market_updates, const string &iface, const string &snapshot_ip, int snapshot_port)
    : snapshot_md_updates_(market_updates), logger_("exchange_snapshot_synthesizer.log"), snapshot_socket_(logger_), order_pool_(ME_MAX_ORDER_IDS) {
        // initializing out multicast socket and setting is_listening -> false (we are producer)
        ASSERT(snapshot_socket_.init(snapshot_ip, iface, snapshot_port, false) >= 0, "Unable to create snapshot mcast socket. error:" + std::string(std::strerror(errno)));
        for(auto &orders: ticker_orders_){
            orders.fill(nullptr);
        }
    }

    SnapshotSynthesizer::~SnapshotSynthesizer() {
        stop();
    }

    void SnapshotSynthesizer::start() {
        run_ = true; 
        // create and run snapshot_synthesizer on a new thread 
        ASSERT(Common::createAndStartThread(-1, "Exchange/SnapshotSynthesizer", [this]() { run(); }) != nullptr, "Failed to start SnapshotSynthesizer thread.");
    }

    void SnapshotSynthesizer::stop() {
        run_ = false;
    }

    auto SnapshotSynthesizer::addToSnapshot(const MDPMarketUpdate *market_update) {
        // get the market_update struct from the MDPMarketUpdate struct 
        const auto &me_market_update = market_update->me_market_update_;
        // get all the orders for that ticker
        auto *orders = &ticker_orders_.at(me_market_update.ticker_id_);
        switch (me_market_update.type_) {
            // create order at the given order id 
            case MarketUpdateType::ADD: {
                // confirm that no order is already there
                auto order = orders->at(me_market_update.order_id_);
                ASSERT(order == nullptr, "Received:" + me_market_update.toString() + " but order already exists:" + (order ? order->toString() : ""));
                // allocate memory for new order
                orders->at(me_market_update.order_id_) = order_pool_.allocate(me_market_update);
            }
            break;
            // modify  order at given order id 
            case MarketUpdateType::MODIFY: {
                // get the already existing order, and has same order id and side 
                auto order = orders->at(me_market_update.order_id_);
                ASSERT(order != nullptr, "");
                ASSERT(order->order_id_ == me_market_update.order_id_, "");
                ASSERT(order->side_ == me_market_update.side_, "");

                // update the qty and price
                order->qty_ = me_market_update.qty_;
                order->price_ = me_market_update.price_;
            }
            break;
            case MarketUpdateType::CANCEL: {
                // verify the alredy existing order
                auto order = orders->at(me_market_update.order_id_);
                ASSERT(order != nullptr, "");
                ASSERT(order->order_id_ == me_market_update.order_id_, "");
                ASSERT(order->side_ == me_market_update.side_, "");

                // deallocate order
                order_pool_.deallocate(order);
                orders->at(me_market_update.order_id_) = nullptr;
            }
            break;
            // nothing needs to be done for the other market update types 
            case MarketUpdateType::SNAPSHOT_START:
            case MarketUpdateType::CLEAR:
            case MarketUpdateType::INVALID:
            case MarketUpdateType::SNAPSHOT_END:
            case MarketUpdateType::TRADE:
            break;
        }

        //  update the sequence numbers
        ASSERT(market_update->seq_num_ == last_inc_seq_num_ + 1, "");
        last_inc_seq_num_ = market_update->seq_num_;
    }

    auto SnapshotSynthesizer::publishSnapshot() {
        size_t snapshot_size = 0;
        // send message to mark the start of the snapshot
        const MDPMarketUpdate start_market_update{snapshot_size++, {
            MarketUpdateType::SNAPSHOT_START,
            last_inc_seq_num_
        }};
        // log
        snapshot_socket_.send(&start_market_update, sizeof(MDPMarketUpdate));

        // iterate over all the tickers
        for(size_t ticker_id = 0; ticker_id < ticker_orders_.size(); ++ticker_id) {
            // get all orders in that ticker
            const auto &orders = ticker_orders_.at(ticker_id);

            // send clear market update type
            MEMarketUpdate me_market_update;
            me_market_update.type_ = MarketUpdateType::CLEAR;
            me_market_update.ticker_id_ = ticker_id;

            const MDPMarketUpdate clear_market_update{snapshot_size++, me_market_update};
            // log 
            snapshot_socket_.send(&clear_market_update, sizeof(MDPMarketUpdate));

            // iterate over all the orders for that ticker
            for(const auto order: orders) {
                if (order) {
                    // send that order and call the callback function
                    const MDPMarketUpdate market_update{snapshot_size++, *order};
                    // log 
                    snapshot_socket_.send(&market_update, sizeof(MDPMarketUpdate));
                    snapshot_socket_.sendAndRecv();
                }
            }
        }

        // send message to mark the end of the snapshot
        const MDPMarketUpdate end_market_update{snapshot_size++, {
            MarketUpdateType::SNAPSHOT_END,
            last_inc_seq_num_
        }};
        // log 
        snapshot_socket_.send(&end_market_update, sizeof(MDPMarketUpdate));
        snapshot_socket_.sendAndRecv();

        // log
    }

    void SnapshotSynthesizer::run() {
        // log 
        while(run_) {
            for(auto market_update = snapshot_md_updates_->getNextToRead(); snapshot_md_updates_->size() && market_update; market_update = snapshot_md_updates_->getNextToRead()){
                // log 
                addToSnapshot(market_update);
                snapshot_md_updates_->updateReadIndex();
            }

            if(getCurrentNanos() - last_snapshot_time_ > 60 * NANOS_TO_SECS) {
                last_snapshot_time_ = getCurrentNanos();
                publishSnapshot();
            }
        }
    }
}