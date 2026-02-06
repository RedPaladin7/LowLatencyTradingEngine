#include "snapshot_synthesizer.h"

namespace Exchange {
    SnapshotSynthesizer::SnapshotSynthesizer(MDPMarketUpdateLFQueue *market_updates, const string &iface, const string &snapshot_ip, int snapshot_port)
    : snapshot_md_updates_(market_updates), logger_("exchange_snapshot_synthesizer.log"), snapshot_socket_(logger_), order_pool_(ME_MAX_ORDER_IDS) {
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
        ASSERT(Common::createAndStartThread(-1, "Exchange/SnapshotSynthesizer", [this]() { run(); }) != nullptr, "Failed to start SnapshotSynthesizer thread.");
    }

    void SnapshotSynthesizer::stop() {
        run_ = false;
    }

    auto SnapshotSynthesizer::addToSnapshot(const MDPMarketUpdate *market_update) {
        const auto &me_market_update = market_update->me_market_update_;
        auto *orders = &ticker_orders_.at(me_market_update.ticker_id_);
        switch (me_market_update.type_) {
            case MarketUpdateType::ADD: {
                auto order = orders->at(me_market_update.order_id_);
                ASSERT(order == nullptr, "Received:" + me_market_update.toString() + " but order already exists:" + (order ? order->toString() : ""));
                orders->at(me_market_update.order_id_) = order_pool_.allocate(me_market_update);
            }
            break;
            case MarketUpdateType::MODIFY: {
                auto order = orders->at(me_market_update.order_id_);
                ASSERT(order != nullptr, "");
                ASSERT(order->order_id_ == me_market_update.order_id_, "");
                ASSERT(order->side_ == me_market_update.side_, "");

                order->qty_ = me_market_update.qty_;
                order->price_ = me_market_update.price_;
            }
            break;
            case MarketUpdateType::CANCEL: {
                auto order = orders->at(me_market_update.order_id_);
                ASSERT(order != nullptr, "");
                ASSERT(order->order_id_ == me_market_update.order_id_, "");
                ASSERT(order->side_ == me_market_update.side_, "");

                order_pool_.deallocate(order);
                orders->at(me_market_update.order_id_) = nullptr;
            }
            break;
            case MarketUpdateType::SNAPSHOT_START:
            case MarketUpdateType::CLEAR:
            case MarketUpdateType::INVALID:
            case MarketUpdateType::SNAPSHOT_END:
            case MarketUpdateType::TRADE:
            break;
        }

        ASSERT(market_update->seq_num_ == last_inc_seq_num_ + 1, "");
        last_inc_seq_num_ = market_update->seq_num_;
    }

    auto SnapshotSynthesizer::publishSnapshot() {

    }

    void SnapshotSynthesizer::run() {

    }
}