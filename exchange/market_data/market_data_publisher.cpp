#include "market_data_publisher.h"

namespace Exchange {
    MarketDataPublisher::MarketDataPublisher(MEMarketUpdateLFQueue *market_updates, const string &iface, const string &snapshot_ip, int snapshot_port, const string &incremental_ip, int incremental_port)
    : outgoing_md_updates_(market_updates), snapshot_md_updates_(ME_MAX_MARKET_UPDATES), run_(false), logger_("exchange_market_data_publisher.log"), incremental_socket_(logger_) {
        // is listening set to false, because we are the producer in the multicast group
        ASSERT(incremental_socket_.init(incremental_ip, iface, incremental_port, false) >= 0, "");
        snapshot_synthesizer_ = new SnapshotSynthesizer(&snapshot_md_updates_, iface, snapshot_ip, snapshot_port);
    }

    auto MarketDataPublisher::run() noexcept -> void {
        // log 
        while(run_) {
            for(auto market_update = outgoing_md_updates_->getNextToRead(); outgoing_md_updates_->size() && market_update; market_update = outgoing_md_updates_->getNextToRead()){
                //log 
                // constantly checks the outgoing_md_updates queue (if any data needs to be sent to market data consumer)
                // send incremental updates to the clients with sequence number (udp connection)
                // populates the outbound_data_ vector of our incremental socket
                incremental_socket_.send(&next_inc_seq_num_, sizeof(next_inc_seq_num_));
                incremental_socket_.send(market_update, sizeof(market_update));
                // updating the read index
                outgoing_md_updates_->updateReadIndex();

                // updates the snapshot_updates queue (not sending data through tcp or udp)
                // same info and sequence number shared
                auto next_write= snapshot_md_updates_.getNextToWriteTo();
                next_write->seq_num_= next_inc_seq_num_;
                next_write->me_market_update_= *market_update;
                snapshot_md_updates_.updateWriteIndex();

                // finally the sequence number is updated
                ++next_inc_seq_num_;
            }
            // data from the outbound_data_ vector is actually sent to the clients 
            incremental_socket_.sendAndRecv();
        }
    }
}