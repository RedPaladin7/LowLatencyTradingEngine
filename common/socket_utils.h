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

// socket creation -> 
// 1. setup flags, then pass it to hints {flags, family, stream, protocol}
// 2. pass it to getaddrinfo(ip, port, hints) -> populates with addrinfo link list
// 3. configure options -> disable nagle, set non blocking, listeni etc.
// 4. server -> bind + listen, client -> connect

namespace Common {
    struct SocketCfg{
        string ip_; // address of device on network to connect to   
        string iface_; // physical/virtual network connection point (eg wifi, eth)
        int port_ = -1; // endpoint specifying service on device
        bool is_udp_ = false;
        bool is_listening_ = false; // is server or not
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

    // returns ip address assigned to given interface, used if ip address is not given directly
    inline auto getIfaceIP(const string &iface) -> string {
        char buf[NI_MAXHOST] = {'\0'};
        ifaddrs *ifaddr = nullptr; // contains info on single network interface 
        // is part of linked list, points to next network if on the device

        if(getifaddrs(&ifaddr)!=-1) { 
            // getifaddrs() gets all network interfaces, assign it to ifaddr (0 on succcess)
            for(ifaddrs *ifa = ifaddr; ifa; ifa = ifa->ifa_next){
                if(ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET && iface == ifa->ifa_name){
                    // look for iface with given name 
                    // AF_INET -> ipv4 address
                    getnameinfo(ifa->ifa_addr, sizeof(sockaddr_in), buf, sizeof(buf), NULL, 0, NI_NUMERICHOST);
                    // convert to human readable string and store in buf
                    break;
                }
            }
            freeifaddrs(ifaddr);
        }
        return buf;
    }

    // thread does not wait if there is not data when read() is called
    // returns immediately with error message
    inline auto setNonBlocking(int fd) -> bool {
        const auto flags = fcntl(fd, F_GETFL, 0); // get flags for the current file descriptor
        if (flags & O_NONBLOCK) return true; // if already non blocking do nothing
        return (fcntl(fd, F_SETFL, flags | O_NONBLOCK)!=-1); // set non blocking
    }

    // nagle -> batch small packets together to save bandwidth (no sending many headers)
    // problem -> we have to wait a bit for batching
    inline auto disableNagle(int fd) -> bool {
        int one = 1;
        return (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<void *>(&one), sizeof(one))!=-1);
    }

    inline auto setSOTimeStamp(int fd) -> bool {
        int one = 1;
        return (setsockopt(fd, SOL_SOCKET, SO_TIMESTAMP, reinterpret_cast<void *>(&one), sizeof(one))!=-1);
    }

    // joins multicast group -> one sender, many recv
    // sender sends one packet to multicast address, network duplicated and sends to all subscribers
    inline auto join(int fd, const string &ip) -> bool {
        const ip_mreq mreq{
            {inet_addr(ip.c_str())}, // the multicast group to join
            {htonl(INADDR_ANY)} // any interface
        };
        return (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq))!=-1);
    }

    // creates socket and returns file descriptor
    [[nodiscard]] inline auto createSocket(Logger &logger, const SocketCfg& socket_cfg) -> int {
        string time_str;
        // get ip using function if not already provided 
        const auto ip = socket_cfg.ip_.empty() ? getIfaceIP(socket_cfg.iface_) : socket_cfg.ip_;
        logger.log("%:% %() % cfg:%\n", __FILE__, __LINE__, __FUNCTION__,
               Common::getCurrentTimeStr(&time_str), socket_cfg.toString());
        // passive if server, ip and port are numeric do not do lookup 
        const int input_flags = (socket_cfg.is_listening_ ? AI_PASSIVE : 0) | (AI_NUMERICHOST | AI_NUMERICSERV);
        const addrinfo hints{ // addrinfo describes socket config
            input_flags,
            AF_INET,
            socket_cfg.is_udp_? SOCK_DGRAM : SOCK_STREAM,
            socket_cfg.is_udp_? IPPROTO_UDP : IPPROTO_TCP,
            0, 0, nullptr
        };

        // converts ip + port to binary socket address structure
        // result populated with addrinfo link list (usually has only one element)
        addrinfo *result = nullptr;
        const auto rc = getaddrinfo(
            ip.c_str(), 
            to_string(socket_cfg.port_).c_str(),
            &hints, 
            &result
        );
        ASSERT(!rc, "getaddrinfo() failed. error:" + string(gai_strerror(rc)) + "errno:" + strerror(errno));

        int socket_fd = -1;
        int one = 1;
        for(addrinfo *rp = result; rp; rp=rp->ai_next) {
            // create socket -> needs family, type, and protocol
            ASSERT((socket_fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol)) != -1, "socket() failed. errno:" + std::string(strerror(errno)));

            ASSERT(setNonBlocking(socket_fd), "setNonBlocking() failed. errno:" + std::string(strerror(errno)));

            if(!socket_cfg.is_udp_) {
                ASSERT(disableNagle(socket_fd), "disableNagle() failed. errno:" + std::string(strerror(errno)));
            }

            // if not server then connect to given ip_
            if(!socket_cfg.is_listening_) {
                ASSERT(connect(socket_fd, rp->ai_addr, rp->ai_addrlen), "connect() failed. errno:" + std::string(strerror(errno)));
            }

            // if server, allow port reuse immediately after crashing
            if(socket_cfg.is_listening_) {
                ASSERT(setsockopt(socket_fd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<void *>(&one), sizeof(one))!=-1, "setsockopt() SO_REUSEADDR failed. errno:" + std::string(strerror(errno)));
            }

            // ip_ represents the address where you listen to if you are a server
            // ip_ represents the address of the server you want to connect to if you are a client
            if(socket_cfg.is_listening_) {
                const sockaddr_in addr{ // where to listen to (we are server)
                    AF_INET,
                    htons(socket_cfg.port_),
                    {htonl(INADDR_ANY)}, {}
                };
                ASSERT(bind(socket_fd, socket_cfg.is_udp_ ? reinterpret_cast<const struct sockaddr *>(&addr) : rp->ai_addr, sizeof(addr))==0, "bind() failed. errno:%" + std::string(strerror(errno)));
                // bind the socket to given port 
            }

            if(!socket_cfg.is_udp_ && socket_cfg.is_listening_) {
                // passively listening for new connections all the time (kernel in background)
                // adds it to the queue 
                // it can have max 1024 pending connection requests at once
                ASSERT(listen(socket_fd, MaxTCPServerBacklog)==0, "listen() failed. errno:" + std::string(strerror(errno)));
            }

            if(socket_cfg.needs_so_timestamp_) {
                ASSERT(setSOTimeStamp(socket_fd), "setSOTimestamp() failed. errno:" + std::string(strerror(errno)));
            }
        }
        return socket_fd;
    }
}