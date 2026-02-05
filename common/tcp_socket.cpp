#include "tcp_socket.h"

// function for basic socket functions -> send and receive

namespace Common {
    auto TCPSocket::connect(const string &ip, const string &iface, int port, bool is_listening) -> int {
        // creates socket and returns socket fd
        // function called connect but works both for server and client 
        // in case of server it will bind and listen
        // in case of client it will just connect
        const SocketCfg socket_cfg{
            ip,
            iface,
            port, 
            false, 
            is_listening,
            true
        };
        socket_fd_ = createSocket(logger_, socket_cfg);

        socket_attrib_.sin_addr.s_addr = INADDR_ANY;
        socket_attrib_.sin_port = htons(port);
        socket_attrib_.sin_family = AF_INET;

        return socket_fd_;
    }

    auto TCPSocket::sendAndRecv() noexcept -> bool {
        // receives incoming data and sends queued outgoing data 
        char ctrl[CMSG_SPACE(sizeof(struct timeval))]; // space ancillary data (timestamps)
        // cast to cmsghdr, still points to the same buffer
        // allows field access like msg level, type and len
        auto cmsg = reinterpret_cast<struct cmsghdr *>(&ctrl); 

        // iov -> specifies where to read from, and maximum number of bytes that can be written 
        // we are reading all the inbound data, hence size if the rest of the buffer
        iovec iov{inbound_data_.data() + next_rcv_valid_index_, TCPBufferSize - next_rcv_valid_index_};
        msghdr msg{ // message header with receive parameters
            &socket_attrib_,
            sizeof(socket_attrib_),
            &iov,
            1,
            ctrl,
            sizeof(ctrl), 0
        };

        // reading data
        // if data is present, it is written to iov, metadata written to ctrl
        const auto read_size = recvmsg(socket_fd_, &msg, MSG_DONTWAIT);
        if(read_size > 0) {
            next_rcv_valid_index_ += read_size;
            Nanos kernel_time = 0;
            timeval time_kernel;
            if(cmsg->cmsg_level == SOL_SOCKET &&
                cmsg->cmsg_type == SCM_TIMESTAMP &&
                cmsg->cmsg_len == CMSG_LEN(sizeof(time_kernel))){
                // copy timestamp
                memcpy(&time_kernel, CMSG_DATA(cmsg), sizeof(time_kernel));
                kernel_time = time_kernel.tv_sec * NANOS_TO_SECS + time_kernel.tv_usec * NANOS_TO_MICROS;
            }
            const auto user_time = getCurrentNanos();
            logger_.log("%:% %() % read socket:% len:% utime:% ktime:% diff:%\n", __FILE__, __LINE__, __FUNCTION__,
            Common::getCurrentTimeStr(&time_str_), socket_fd_, next_rcv_valid_index_, user_time, kernel_time, (user_time - kernel_time));
            // call the callback function, pass current socket 
            recv_callback_(this, kernel_time);
        }

        // handing EPOLLERRR / EPOLLHUP
        if (read_size == 0 || (read_size < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
             logger_.log("%:% %() % socket:% disconnected or errored, read_size:% errno:%\n", 
                    __FILE__, __LINE__, __FUNCTION__, 
                    Common::getCurrentTimeStr(&time_str_), socket_fd_, read_size, strerror(errno));
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }

        // sending data 
        if(next_send_valid_index_ > 0) {
            // sending data from the start of the outbound data buffer to next send index
            const auto n = ::send(socket_fd_, outbound_data_.data(), next_send_valid_index_, MSG_DONTWAIT | MSG_NOSIGNAL);
            logger_.log("%:% %() % send socket:% len:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket_fd_, n);
        }
        next_send_valid_index_ = 0; // reset send index
        return (read_size > 0); // returns bool on whether data was read or not
    }

    // copies data to outbound buffer, does not send it yet
    auto TCPSocket::send(const void *data, size_t len) noexcept -> void {
        memcpy(outbound_data_.data() + next_send_valid_index_, data, len);
        next_send_valid_index_+= len;
    }
}