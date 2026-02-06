#pragma once 

#include "tcp_socket.h"
using namespace std;

namespace Common {
    struct TCPServer{
        explicit TCPServer(Logger &logger): listener_socket_(logger), logger_(logger) {}

        auto listen(const string &iface, int port) -> void;

        auto poll() noexcept -> void;

        auto sendAndRecv() noexcept -> void;

        private:
        auto addToEpollList(TCPSocket *socket);


        public:
        int epoll_fd_ = -1;
        // only job of listener socket is to accept new connections
        TCPSocket listener_socket_;

        epoll_event events_[1024];
        vector<TCPSocket *> receive_sockets_, send_sockets_;
        function<void(TCPSocket *s, Nanos rx_time)> recv_callback_ = nullptr;
        function<void()> recv_finished_callback_ = nullptr;

        string time_str_;
        Logger &logger_;
    };
}