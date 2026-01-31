#pragma once 

#include <functional>
#include "socket_utils.h"
#include "logging.h"

using namespace std;

namespace Common {
    constexpr size_t McastBufferSize = 64 * 1024 * 1024;

    struct McastSocket {
        McastSocket(Logger &logger): logger_(logger) {
            outbound_data_.resize(McastBufferSize);
            inbound_data_.resize(McastBufferSize);
        }

        auto init(const string &ip, const string &iface, int port, bool is_listening_)->int;

        auto join(const string &ip)->bool;

        auto leave(const string &ip, int port)->void;

        auto sendAndRecv() noexcept -> bool;

        auto send(const void *data, size_t len) noexcept -> void;

        int socket_fd_ = -1;

        vector<char> outbound_data_;
        size_t next_send_valid_index_ = 0;
        vector<char> inbound_data_;
        size_t next_rcv_valid_index_ = 0;

        function<void(McastSocket *s)> recv_callback_ = nullptr;
        string time_str_;
        Logger &logger_;
    };
}