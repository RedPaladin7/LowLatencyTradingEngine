#include <csignal>

#include "matcher/matching_engine.h"
#include "market_data/market_data_publisher.h"
#include "order_server/order_server.h"

using namespace std;

Common::Logger *logger = nullptr;
Exchange::MatchingEngine *matching_engine = nullptr;
Exchange::MarketDataPublisher *market_data_publisher = nullptr;
Exchange::OrderServer *order_server = nullptr;

void signal_handler(int) {
    using namespace literals::chrono_literals;
    this_thread::sleep_for(10s);

    delete logger;
    logger = nullptr;
    delete matching_engine;
    matching_engine = nullptr;
    delete market_data_publisher;
    market_data_publisher = nullptr;
    delete order_server;
    order_server = nullptr;

    this_thread::sleep_for(10s);
    exit(EXIT_SUCCESS);
}

int main(int, char **) {
    logger = new Common::Logger("exchange_main.log");

    signal(SIGINT, signal_handler);
    const int sleep_time = 100 * 1000;

    Exchange::ClientRequestLFQueue client_requests(ME_MAX_CLIENT_UPDATES);
    Exchange::ClientResponseLFQueue client_responses(ME_MAX_CLIENT_UPDATES);
    Exchange::MEMarketUpdateLFQueue market_updates(ME_MAX_MARKET_UPDATES);

    string time_str;

    // log 
    matching_engine = new Exchange::MatchingEngine(&client_requests, &client_responses, &market_updates);
    matching_engine->start();

    const string mkt_pub_iface = "lo";
    const string snap_pub_ip = "233.252.14.1", inc_pub_ip = "233.252.14.3";
    const int snap_pub_port = 20000, inc_pub_port = 20001;

    // log 
    market_data_publisher = new Exchange::MarketDataPublisher(&market_updates, mkt_pub_iface, snap_pub_ip, snap_pub_port, inc_pub_ip, inc_pub_port);
    market_data_publisher->start();

    const string order_gw_iface = "lo";
    const int order_gw_port = 12345;

    // log 
    order_server = new Exchange::OrderServer(&client_requests, &client_responses, order_gw_iface, order_gw_port);
    order_server->start();

    while(true) {
        // log 
        usleep(sleep_time * 1000);
    }
}