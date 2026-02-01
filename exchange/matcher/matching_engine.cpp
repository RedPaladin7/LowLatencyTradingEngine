#include "matching_engine.h"

using namespace std;

namespace Exchange {
    MatchingEngine::MatchingEngine(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses, MEMarketUpdateLFQueue *market_updates):
        incoming_requests_(client_requests),
        outgoing_ogw_responses_(client_responses),
        outgoing_md_updates_(market_updates),
        logger_("exchange_matching_engine.log") {
        // todo 
    }

    MatchingEngine::~MatchingEngine() {
        run_ = false;

        using namespace literals::chrono_literals;
        this_thread::sleep_for(1s);

        incoming_requests_ = nullptr;
        outgoing_ogw_responses_ = nullptr;
        outgoing_md_updates_ = nullptr;

        // for loop 
    }

    auto MatchingEngine::start() -> void {
        run_ = true;
        ASSERT(Common::createAndStartThread(-1, "Exchange/MatchingEngine", [this](){run();}) != nullptr, "Failed to start MatchingEngine thread.");
    }

    auto MatchingEngine::stop() -> void {
        run_ = false;
    }
}