#include <csignal>
#include <iostream>
#include "matcher/matching_engine.h"

using namespace std;

Common::Logger* logger = nullptr;
Exchange::MatchingEngine* matching_engine = nullptr;

// Clean shutdown handler
void signal_handler(int) {
    cout << "Shutting down..." << endl;
    if (matching_engine) {
        matching_engine->stop();
        delete matching_engine;
    }
    if (logger) delete logger;
    exit(EXIT_SUCCESS);
}

int main(int, char**) {
    logger = new Common::Logger("exchange_main.log");
    signal(SIGINT, signal_handler);

    // Initialize Lock-Free Queues
    Exchange::ClientRequestLFQueue client_requests(ME_MAX_CLIENT_UPDATES);
    Exchange::ClientResponseLFQueue client_responses(ME_MAX_CLIENT_UPDATES);
    Exchange::MEMarketUpdateLFQueue market_updates(ME_MAX_MARKET_UPDATES);

    // 1. Start the Matching Engine (spawns the background thread)
    matching_engine = new Exchange::MatchingEngine(&client_requests, &client_responses, &market_updates);
    matching_engine->start();

    string time_str;
    logger->log("%:% %() % Matching Engine Started. Injecting test orders...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));

    // 2. Inject a BUY order
    auto* req1 = client_requests.getNextToWriteTo();
    req1->type_ = Exchange::ClientRequestType::NEW;
    req1->client_id_ = 1;
    req1->ticker_id_ = 1;
    req1->order_id_ = 1001;
    req1->side_ = Common::Side::BUY;
    req1->price_ = 500;
    req1->qty_ = 100;
    client_requests.updateWriteIndex();

    // 3. Inject a matching SELL order
    auto* req2 = client_requests.getNextToWriteTo();
    req2->type_ = Exchange::ClientRequestType::NEW;
    req2->client_id_ = 2;
    req2->ticker_id_ = 1;
    req2->order_id_ = 2001;
    req2->side_ = Common::Side::SELL;
    req2->price_ = 500; // Match price
    req2->qty_ = 100;
    client_requests.updateWriteIndex();

    // 4. Monitor the response queue in the main thread
    // This simulates what an Order Gateway would do
    while (true) {
        // Check for responses (FILLED, ACCEPTED, etc.)
        const auto* response = client_responses.getNextToRead();
        if (response) {
            cout << "CLIENT RESPONSE: " << response->toString() << endl;
            client_responses.updateReadIndex();
        }

        // Check for market updates (ADD, TRADE, etc.)
        const auto* mkt_update = market_updates.getNextToRead();
        if (mkt_update) {
            cout << "MARKET UPDATE: " << mkt_update->toString() << endl;
            market_updates.updateReadIndex();
        }

        // Small sleep to prevent 100% CPU usage in this monitor loop
        usleep(1000); 
    }

    return 0;
}