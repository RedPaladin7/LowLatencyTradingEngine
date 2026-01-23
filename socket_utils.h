#pragma once

#include <iostream>
#include <string>
#include <unordered_set>
#include <sstream>
#include <sys/epoll.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <sys/socket.h>
#include <fcntl.h>

#include "macros.h"

#include "logging.h"
using namespace std;

namespace Common {
    struct SocketCfg{
        string ip_; 
        string iface_;
        int port_ = -1;
        bool is_udp_ = false;
        bool is_listening_ = false;
        bool needs_so_timestamp_ = false;

        auto toString() const {
            std::stringstream ss;
            ss << "SocketCfg[ip:" << ip_
            << " iface:" << iface_
            << " port:" << port_
            << " is_udp:" << is_udp_
            << " is_listening:" << is_listening_
            << " needs_SO_timestamp:" << needs_so_timestamp_
            << "]";

            return ss.str();
        }
    };

    constexpr int MaxTCPServerBacklog = 1024;

    inline auto getIfaceIP(const string &iface) -> string {
        char buf[NI_MAXHOST] = {'\0'};
        ifaddrs *ifaddr = nullptr; 
    }
}