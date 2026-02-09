#pragma once 

#include <functional>
#include "market_data/snapshot_synthesizer.h"

using namespace std;

namespace Exchange {
    class MarketDataPublisher {
    public: 
        MarketDataPublisher(MEMarketUpdateLFQueue *market_updates, const string &iface, const string &snapshot_ip, int snapshot_port, const string &incremental_ip, int incremental_port);

        ~MarketDataPublisher() {
            stop();

            using namespace::literals::chrono_literals;
            this_thread::sleep_for(5s);
            snapshot_synthesizer_ = nullptr;
        }

        auto start() {
            run_ = true;

            ASSERT(Common::createAndStartThread(-1, "Exchange/MarketDataPublisher", [this]() { run(); }) != nullptr, "Failed to start MarketData thread.");
        }

        auto stop() -> void {
            run_ = false;
            snapshot_synthesizer_->stop();
        }

        auto run() noexcept -> void;
    
    private: 
        size_t next_inc_seq_num_ = 1;
        // incremental messages going to the clients through udp
        MEMarketUpdateLFQueue *outgoing_md_updates_ = nullptr;
        // same information being sent to the snapshot synthesizer (will be read through the shared queue)
        MDPMarketUpdateLFQueue snapshot_md_updates_;
        
        volatile bool run_ = false;

        string time_str_;
        Logger logger_;

        Common::McastSocket incremental_socket_;
        SnapshotSynthesizer *snapshot_synthesizer_ = nullptr;
    };
}