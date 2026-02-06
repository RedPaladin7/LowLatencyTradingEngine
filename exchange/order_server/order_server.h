#pragma once 

#include <functional>

#include "common/thread_utils.h"
#include "common/macros.h"
#include "common/tcp_server.h"

#include "order_server/client_request.h"
#include "order_server/client_response.h"
#include "fifo_sequencer.h"

using namespace std;

namespace Exchange {
    class OrderServer {
    public:
        OrderServer(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses, const string &iface, int port);

        ~OrderServer();

        auto start() -> void;

        auto stop() -> void;

        auto run() noexcept {
            // log 
            while(run_) {
                // poll for socket activity, fill the events_ array
                tcp_server_.poll();
                // process the incoming and outgoing data, for send_sockets_ and receive_sockets_
                tcp_server_.sendAndRecv();

                // iterate over the outgoing responses array
                for(auto client_response = outgoing_responses_->getNextToRead(); outgoing_responses_->size() && client_response; client_response = outgoing_responses_->getNextToRead()){
                    // get the next outgoing sequence number for that client
                    auto &next_outgoing_seq_num = cid_next_outgoing_seq_num_[client_response->client_id_];
                    // log 

                    ASSERT(cid_tcp_socket_[client_response->client_id_] != nullptr, "");
                    // first send the sequence number
                    cid_tcp_socket_[client_response->client_id_]->send(&next_outgoing_seq_num, sizeof(next_outgoing_seq_num));
                    // then send the actuall data 
                    cid_tcp_socket_[client_response->client_id_]->send(client_response, sizeof(MEClientResponse));

                    outgoing_responses_->updateReadIndex();
                    ++next_outgoing_seq_num;
                }
            }
        }

        auto recvCallback(TCPSocket *socket, Nanos rx_time) noexcept {
            // log 
            if (socket->next_rcv_valid_index_ >= sizeof(OMClientRequest)) {
                size_t i =0;
                for(; i + sizeof(OMClientRequest) <= socket->next_rcv_valid_index_; i += sizeof(OMClientRequest)) {
                    // get the request from inbound data
                    auto request = reinterpret_cast<const OMClientRequest *>(socket->inbound_data_.data() + i);
                    // log 

                    // if socket does not exist for that client, add one
                    if(UNLIKELY(cid_tcp_socket_[request->me_client_request.client_id_] == nullptr)) {
                        cid_tcp_socket_[request->me_client_request.client_id_] = socket;
                    }

                    // wrong socket -> skip
                    if(cid_tcp_socket_[request->me_client_request.client_id_] != socket) {
                        // log 
                        continue;
                    }

                    // check if the sequence number of that message is the same one that we are expecting to receive
                    auto &next_exp_seq_num = cid_next_exp_seq_num_[request->me_client_request.client_id_];
                    if(request->seq_num_ != next_exp_seq_num) {
                        // log 
                        continue;
                    }

                    ++next_exp_seq_num;
                    // forward the client request to fifo sequencer
                    fifo_sequencer_.addClientRequest(rx_time, request->me_client_request);
                }
                memcpy(socket->inbound_data_.data(), socket->inbound_data_.data() + i, socket->next_rcv_valid_index_ - i);
                socket->next_rcv_valid_index_ -= i;
            }
        }

        auto recvFinishedCallback() noexcept {
            fifo_sequencer_.sequenceAndPublish();
        }

        OrderServer() = delete;
        OrderServer(const OrderServer &) = delete;
        OrderServer(const OrderServer &&) = delete;
        OrderServer &operator=(const OrderServer &) = delete;
        OrderServer &operator=(const OrderServer &&) = delete;

    private:
        const string iface_;
        const int port_;

        ClientResponseLFQueue *outgoing_responses_ = nullptr;
        volatile bool run_ = false;

        string time_str_;
        Logger logger_; 

        array<size_t, ME_MAX_NUM_CLIENTS> cid_next_outgoing_seq_num_;
        array<size_t, ME_MAX_NUM_CLIENTS> cid_next_exp_seq_num_;

        array<Common::TCPSocket *, ME_MAX_NUM_CLIENTS> cid_tcp_socket_;

        Common::TCPServer tcp_server_;
        
        FIFOSequencer fifo_sequencer_;
    };
}