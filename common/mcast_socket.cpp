#include "mcast_socket.h"
using namespace std;

namespace Common {
    // creates a socket on a new thread, not multicast yet, just a normal socket 
    auto McastSocket::init(const string &ip, const string &iface, int port, bool is_listening)->int {
        // is_listening = true -> socket is a consumer, listening to a multicast group 
        // is_listening = false -> socket is the producer 
        // is_udp -> set to true, multicast updates handled through udp not tcp
        const SocketCfg socket_cfg{
            ip, 
            iface,
            port, 
            true,
            is_listening,
            false
        }; // does not need timestamps
        socket_fd_ = createSocket(logger_, socket_cfg);
        return socket_fd_;
    }

    bool McastSocket::join(const string &ip) {
        // joining a multicast group, calls join() in socket_utils.h file 
        // can be called only if is_listening = true
        return Common::join(socket_fd_, ip);
    }

    auto McastSocket::leave(const string &, int) -> void {
        // close the socket and end the subscription
        close(socket_fd_);
        socket_fd_ = -1;
    }

    // callback function is only called when we receive the data 
    auto McastSocket::sendAndRecv() noexcept -> bool {
        // read the data buffered in the inbound_data_ vector 
        const ssize_t n_rcv = recv(socket_fd_, inbound_data_.data() + next_rcv_valid_index_, McastBufferSize - next_rcv_valid_index_, MSG_DONTWAIT);

        if(n_rcv > 0) {
            next_rcv_valid_index_ += n_rcv;
            logger_.log("%:% %() % read socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket_fd_, next_rcv_valid_index_);
            // update the recv index and call the callback function
            recv_callback_(this);
        }

        // send data buffered in the outbound_data_ vector 
        if(next_send_valid_index_ > 0) {
            ssize_t n = ::send(socket_fd_, outbound_data_.data(), next_send_valid_index_, MSG_DONTWAIT | MSG_NOSIGNAL);
            logger_.log("%:% %() % send socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket_fd_, n);
        }
        next_send_valid_index_ = 0;
        // return true if any data was read 
        return (n_rcv > 0);
    }

    // copies data in the outbound_data_, does not send it immediately 
    auto McastSocket::send(const void *data, size_t len) noexcept -> void {
        memcpy(outbound_data_.data() + next_send_valid_index_, data, len);
        next_send_valid_index_ += len;
        ASSERT(next_send_valid_index_ < McastBufferSize, "Mcast buffer filled up and sendAndRecv() not called.");
    }
}