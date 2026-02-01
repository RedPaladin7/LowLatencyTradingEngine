#pragma once 

#include "common/types.h"
#include "common/lf_queue.h"
#include <sstream>

using namespace Common;
using namespace std;

namespace Exchange {
#pragma pack(push, 1)
    
    enum class ClientRequestType: uint8_t {
        INVALID = 0,
        NEW = 1,
        CANCEL = 2
    };

    inline string clientRequestTypeToString(ClientRequestType type) {
        switch(type) {
            case ClientRequestType::INVALID:
                return "INVALID";
            case ClientRequestType::NEW:
                return "NEW";
            case ClientRequestType::CANCEL:
                return "CANCEL";
        }
        return "UNKOWN";
    }

    struct MEClientRequest {
        ClientRequestType type_ = ClientRequestType::INVALID;
        ClientId client_id_ = ClientId_INVALID;
        TickerId ticker_id_ = TickerId_INVALID;
        OrderId order_id_ = OrderId_INVALID;
        Side side_ = Side::INVALID;
        Price price_ = Price_INVALID;
        Qty qty_ = Qty_INVALID;

        auto toString() const {
            stringstream ss;
            ss << "MEClientRequest"
               << " ["
               << " type:" << clientRequestTypeToString(type_)
               << " client:" << clientIdToString(client_id_)
               << " ticker:" << tickerIdToString(ticker_id_)
               << " oid:" << orderIdToString(order_id_)
               << " side:" << sideToString(side_)
               << " price:" << priceToString(price_)
               << " qty:" << qtyToString(qty_)
               << "]";
            return ss.str();
        }
    };
#pragma pack(pop)
    
    typedef LFQueue<MEClientRequest> ClientRequestLFQueue;
}