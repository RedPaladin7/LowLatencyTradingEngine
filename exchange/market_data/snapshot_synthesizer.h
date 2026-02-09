#pragma once 

#include "common/types.h"
#include "common/thread_utils.h"
#include "common/lf_queue.h"
#include "macros.h"
#include "common/mcast_socket.h"
#include "common/mem_pool.h"
#include "common/logging.h"

#include "market_data/market_update.h"
#include "matcher/me_order.h"

using namespace Common;
using namespace std;

namespace Exchange {
    class SnapshotSynthesizer {
    public: 
        SnapshotSynthesizer(MDPMarketUpdateLFQueue *market_updates, const string &iface, const string &snapshot_ip, int snapshot_port);

        ~SnapshotSynthesizer();

        auto start() -> void;

        auto stop() -> void; 

        auto addToSnapshot(const MDPMarketUpdate *market_update);

        auto publishSnapshot();

        auto run() -> void;

        SnapshotSynthesizer() = delete;
        SnapshotSynthesizer(const SnapshotSynthesizer &) = delete;
        SnapshotSynthesizer(const SnapshotSynthesizer &&) = delete;
        SnapshotSynthesizer &operator=(const SnapshotSynthesizer &) = delete;
        SnapshotSynthesizer &operator=(const SnapshotSynthesizer &&) = delete;

    private:
        // queue of updates shared between market_data_publisher and us 
        // mdp will write to the queue and we will read from the queue
        MDPMarketUpdateLFQueue *snapshot_md_updates_ = nullptr;

        Logger logger_;
        volatile bool run_ = false;
        string time_str_;

        // we are the producer in the multicast group 
        // send messages to anyone who subscribes to the snapshot_synthesizer 
        McastSocket snapshot_socket_;

        // array of orders for each ticker 
        array<array<MEMarketUpdate *, ME_MAX_ORDER_IDS>, ME_MAX_ORDER_IDS> ticker_orders_;
        size_t last_inc_seq_num_ = 0;
        Nanos last_snapshot_time_ = 0;

        MemPool<MEMarketUpdate> order_pool_;
    };
}