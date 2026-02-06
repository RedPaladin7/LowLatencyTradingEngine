#include "tcp_server.h"
using namespace std;

// epoll -> monitoring multiple file descriptors to see if IO operations are possible on any of them
// adding for epoll monitoring -> keep an eye on the socket for any events

// poll -> check activity at that instance
// sendAndRecv -> resolve sending and accepting data 

namespace Common {
    auto TCPServer::addToEpollList(TCPSocket *socket) { 
        epoll_event ev {
            // EPOLLET -> edge triggered mode notify only if new data arrives
            // EPOLLIN -> data arrived for read
            EPOLLET | EPOLLIN,
            {reinterpret_cast<void *>(socket)}
        };
        // add socket to monitoring list of epoll_fd_
        return !epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, socket->socket_fd_, &ev);
    }

    auto TCPServer::listen(const string &iface, int port) -> void {
        epoll_fd_ = epoll_create(1); // create epoll instance
        ASSERT(epoll_fd_ >= 0, "epoll_create() failed error:" + string(strerror(errno)));
        // set the server, is_listening_ param = true, ip will be configured automatically
        ASSERT(listener_socket_.connect("", iface, port, true) >= 0, "Listener socket failed to connect. iface:" + iface + " port:" + to_string(port) + " error:" + string(strerror(errno)));
        // our listening endpoint also has to be added to epoll list
        ASSERT(addToEpollList(&listener_socket_), "epoll_ctl() failed. error:" + string(strerror(errno)));
    }

    auto TCPServer::sendAndRecv() noexcept -> void {
        auto recv = false;

        // cleaning up dead sockets from recv sockets
        receive_sockets_.erase(remove_if(receive_sockets_.begin(), receive_sockets_.end(), [](auto socket){
            return socket->socket_fd_ == -1;
        }), receive_sockets_.end());

        for_each(receive_sockets_.begin(), receive_sockets_.end(), [&recv](auto socket){
            // set recv to true if there is some data to read from some socket 
            recv |= socket->sendAndRecv();
        });
        if (recv) {
            // finish callback only called when we read data
            recv_finished_callback_();
        }

        // clean up dead sockets from send sockets
        send_sockets_.erase(remove_if(send_sockets_.begin(), send_sockets_.end(), [](auto socket){
            return socket->socket_fd_ == -1;
        }), send_sockets_.end());

        for_each(send_sockets_.begin(), send_sockets_.end(), [](auto socket) {
            // send data to corresponding sockets
            socket->sendAndRecv();
        });
    }

    // checks which socket have activity and organizes them for processing 
    // at the given instance poll is called
    auto TCPServer::poll() noexcept -> void {
        const int max_events = 1 + send_sockets_.size() + receive_sockets_.size();

        // returns number of socket which have new eveents, populates events_ array
        // gets snapshot of activity at that instant
        // for new events, we have to call this again
        const int n = epoll_wait(epoll_fd_, events_, max_events, 0);
        bool have_new_connection = false;
        for(int i=0; i<n; ++i){
            // iterate over all the events
            const auto &event = events_[i];
            // event.data.ptr stores the socket from which the event is from
            auto socket = reinterpret_cast<TCPSocket *>(event.data.ptr);

            if(event.events & EPOLLIN) {
                // EPOLLIN -> the socket has data available for us to read
                if(socket == &listener_socket_) {
                    // event at our listening socket -> new socket is trying to join us
                    logger_.log("%:% %() % EPOLLIN listener_socket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);
                    have_new_connection = true;
                    continue;
                }
                logger_.log("%:% %() % EPOLLIN socket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);
                // check if the socket is already in receive_sockets, if not, add to it 
                if(find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end()) {
                    receive_sockets_.push_back(socket);
                }
            }
            if(event.events & EPOLLOUT) {
                // EPOLLOUT -> socket is ready to accept data from us
                logger_.log("%:% %() % EPOLLOUT socket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);
                // check is the socket is already in send_sockets, if not , add to it
                if(find(send_sockets_.begin(), send_sockets_.end(), socket) == send_sockets_.end()) {
                    send_sockets_.push_back(socket);
                }
            }
            if(event.events & (EPOLLERR | EPOLLHUP)) {
                // there was an error condition or the socket hung up
                logger_.log("%:% %() % EPOLLERR socket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket->socket_fd_);
                // still add to recieve sockets, recv will return false
                if(find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end()) {
                    receive_sockets_.push_back(socket);
                }
            }

            // keeps calling accept until it returns -1 
            while(have_new_connection) {
                logger_.log("%:% %() % have_new_connection\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
                sockaddr_storage addr;
                socklen_t addr_len = sizeof(addr);
                // accept the new connection, from our listener socket file descriptor
                // new socket fd, representing our connection with that client 
                int fd = accept(listener_socket_.socket_fd_, reinterpret_cast<sockaddr *>(&addr), &addr_len);
                if(fd == -1) break;

                // set socket configs
                ASSERT(setNonBlocking(fd) && disableNagle(fd), "Failed to set non-blocking or no-delay on socket:" + to_string(fd));
                logger_.log("%:% %() % accepted socket:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), fd);

                // create a new socket and assign it the socket fd obtained from accept() call
                auto socket = new TCPSocket(logger_);
                socket->socket_fd_ = fd;
                // gets copy of server's recv_callback function 
                socket->recv_callback_ = recv_callback_;
                // start monitoring this socket 
                ASSERT(addToEpollList(socket), "Unable to add socket. error:" + std::string(std::strerror(errno)));

                // by default add it to receive sockets
                if(find(receive_sockets_.begin(), receive_sockets_.end(), socket) == receive_sockets_.end()){
                    receive_sockets_.push_back(socket);
                }
            }
        }
    }
}