#pragma once  

#include "common/thread_utils.h"
#include "common/lf_queue.h"
#include "common/macros.h"
#include "common/logging.h"

#include "order_server/client_request.h"
#include "order_server/client_response.h"
#include "market_data/market_update.h"

#include "me_order_book.h"

using namespace std;

namespace Exchange {
    class MatchingEngine final {
    public:
        MatchingEngine(ClientRequestLFQueue *client_requests,
                       ClientResponseLFQueue *client_responses,
                       MEMarketUpdateLFQueue *market_updates);
        ~MatchingEngine();

        auto start() -> void; 

        auto stop() -> void;

        MatchingEngine() = delete;
        MatchingEngine(const MatchingEngine &) = delete;
        MatchingEngine(const MatchingEngine &&) = delete;
        MatchingEngine &operator=(const MatchingEngine &) = delete;
        MatchingEngine &operator=(const MatchingEngine &&) = delete;

        auto processClientRequest(const MEClientRequest *client_request) noexcept {

        }

        auto sendClientResponse(const MEClientResponse *client_response) noexcept {
            // log 
            auto next_write = outgoing_ogw_responses_->getNextToWriteTo();
            *next_write = move(*client_response);
            outgoing_ogw_responses_->updateWriteIndex();
        }

        auto sendMarketUpdate(const MEMarketUpdate *market_update) noexcept {
            // log 
            auto next_write = outgoing_md_updates_->getNextToWriteTo();
            *next_write = *market_update;
            outgoing_md_updates_->updateWriteIndex();
        }

        auto run() noexcept {
            // log 
            while(run_) {
                const auto me_client_request = incoming_requests_->getNextToRead(); 
                if(LIKELY(me_client_request)) {
                    // log
                    processClientRequest(me_client_request);
                    incoming_requests_->updateReadIndex();
                }
            }
        }

    private:
        OrderBookHashMap ticker_order_book_;
        ClientRequestLFQueue *incoming_requests_ = nullptr;
        ClientResponseLFQueue *outgoing_ogw_responses_ = nullptr;
        MEMarketUpdateLFQueue *outgoing_md_updates_ = nullptr;

        volatile bool run_ = false;

        string time_str_;
        Logger logger_;
    };
}