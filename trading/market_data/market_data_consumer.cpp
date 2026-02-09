#include "market_data_consumer.h"

namespace Trading {
    MarketDataConsumer::MarketDataConsumer(Common::ClientId client_id, Exchange::MEMarketUpdateLFQueue *market_updates, const string &iface, const string &snapshot_ip, int snapshot_port, const string &incremental_ip, int incremental_port)
    : incoming_md_updates_(market_updates), run_(false), logger_("trading_market_data_consumer_" + to_string(client_id) + ".log"), incremental_mcast_socket_(logger_), snapshot_mcast_socket_(logger_), iface_(iface), snapshot_ip_(snapshot_ip), snapshot_port_(snapshot_port) {
        auto recv_callback = [this](auto socket) {
            recvCallback(socket);
        };

        incremental_mcast_socket_.recv_callback_ = recv_callback;
        ASSERT(incremental_mcast_socket_.init(incremental_ip, iface, incremental_port, true) >= 0, "");

        ASSERT(incremental_mcast_socket_.join(incremental_ip), "");

        snapshot_mcast_socket_.recv_callback_ = recv_callback;
    }

    auto MarketDataConsumer::run() noexcept -> void {
        // log 
        while(run_) {
            incremental_mcast_socket_.sendAndRecv();
            snapshot_mcast_socket_.sendAndRecv();
        }
    }

    auto MarketDataConsumer::startSnapshotSync() -> void {
        snapshot_queued_msgs_.clear();
        incremental_queued_msgs.clear();

        ASSERT(snapshot_mcast_socket_.init(snapshot_ip_, iface_, snapshot_port_, true) >= 0, "");
        ASSERT(snapshot_mcast_socket_.join(snapshot_ip_), "");
    }

    auto MarketDataConsumer::checkSnapshotSync() -> void {
        if(snapshot_queued_msgs_.empty()) {
            return;
        }

        const auto &first_snapshot_msg = snapshot_queued_msgs_.begin()->second; 
        if(first_snapshot_msg.type_ != Exchange::MarketUpdateType::SNAPSHOT_START) {
            // log 
            snapshot_queued_msgs_.clear();
            return;
        }

        vector<Exchange::MEMarketUpdate> final_events;

        auto have_complete_snapshot = true;
        size_t next_snapshot_seq = 0;

        for(auto &snapshot_itr: snapshot_queued_msgs_) {
            // log 
            if(snapshot_itr.first != next_snapshot_seq) {
                have_complete_snapshot = false;
                // log 
                break;
            }
            if(snapshot_itr.second.type_ != Exchange::MarketUpdateType::SNAPSHOT_START && snapshot_itr.second.type_ != Exchange::MarketUpdateType::SNAPSHOT_END) {
                final_events.push_back(snapshot_itr.second);
                ++next_snapshot_seq;
            }
        }

        if(!have_complete_snapshot) {
            // log 
            snapshot_queued_msgs_.clear();
            return;
        }

        const auto &last_snapshot_msg = snapshot_queued_msgs_.rbegin()->second;
        if(last_snapshot_msg.type_ != Exchange::MarketUpdateType::SNAPSHOT_END) {
            // log
            return;
        }

        auto have_complete_incremental = true;
        size_t num_incrementals = 0;
        next_exp_inc_seq_num = last_snapshot_msg.order_id_ + 1;

        for(auto inc_itr = incremental_queued_msgs.begin(); inc_itr != incremental_queued_msgs.end(); ++inc_itr) {
            // log 
            if(inc_itr->first < next_exp_inc_seq_num) continue;

            if(inc_itr->first != next_exp_inc_seq_num) {
                // log 
                have_complete_incremental = false;
                break;
            }
            // log 

            if(inc_itr->second.type_ != Exchange::MarketUpdateType::SNAPSHOT_START && inc_itr->second.type_ != Exchange::MarketUpdateType::SNAPSHOT_END) {
                final_events.push_back(inc_itr->second);
            }
            ++next_exp_inc_seq_num;
            ++num_incrementals;
        }

        if(!have_complete_incremental) {
            // log 
            snapshot_queued_msgs_.clear();
            return;
        }

        for(const auto &itr: final_events) {
            auto next_write = incoming_md_updates_->getNextToWriteTo();
            *next_write = itr;
            incoming_md_updates_->updateWriteIndex();
        }

        // log 

        snapshot_queued_msgs_.clear();
        incremental_queued_msgs.clear();
        in_recovery_ = false;

        snapshot_mcast_socket_.leave(snapshot_ip_, snapshot_port_);
    }

    auto MarketDataConsumer::queueMessage(bool is_snapshot, const Exchange::MDPMarketUpdate *request) {
        if(is_snapshot) {
            if(snapshot_queued_msgs_.find(request->seq_num_) != snapshot_queued_msgs_.end()) {
                // log 
                snapshot_queued_msgs_.clear();
            }
            snapshot_queued_msgs_[request->seq_num_] = request->me_market_update_;
        } else {
            incremental_queued_msgs[request->seq_num_] = request->me_market_update_;
        }
        // log 
        checkSnapshotSync();
    }

    auto MarketDataConsumer::recvCallback(McastSocket *socket) noexcept -> void {
        const auto is_snapshot = (socket->socket_fd_ == snapshot_mcast_socket_.socket_fd_);
        if(UNLIKELY(is_snapshot && !in_recovery_)){
            socket->next_rcv_valid_index_ = 0;
            // log 
            return;
        }

        if(socket->next_rcv_valid_index_ >= sizeof(Exchange::MDPMarketUpdate)) {
            size_t i =0;
            for(; i+sizeof(Exchange::MDPMarketUpdate) <= socket->next_rcv_valid_index_; i += sizeof(Exchange::MDPMarketUpdate)) {
                auto request = reinterpret_cast<const Exchange::MDPMarketUpdate *>(socket->inbound_data_.data() + i);
                // log 
                const bool already_in_recovery = in_recovery_;
                in_recovery_ = (already_in_recovery || request->seq_num_ != next_exp_inc_seq_num);

                if(UNLIKELY(in_recovery_)) {
                    if(UNLIKELY(!already_in_recovery)) {
                        // log
                        startSnapshotSync();
                    }
                    queueMessage(is_snapshot, request);
                } else if(!is_snapshot) {
                    // log 
                    ++next_exp_inc_seq_num;

                    auto next_write = incoming_md_updates_->getNextToWriteTo();
                    *next_write = move(request->me_market_update_);
                    incoming_md_updates_->updateWriteIndex();
                }
            }
            memcpy(socket->inbound_data_.data(), socket->inbound_data_.data() + i, socket->next_rcv_valid_index_ - i);
            socket->next_rcv_valid_index_ -= i;
        }
    }
}