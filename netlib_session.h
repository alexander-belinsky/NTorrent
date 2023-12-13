#pragma once
#include "netlib_header.h"
#include "netlib_message.h"
#include "netlib_safequeue.h"
#include "netlib_natkiller.h"

std::string STUN_HOST = "stun.l.google.com";
uint32_t STUN_PORT = 19302;

namespace netlib {
    template<typename T>
    class Session : public std::enable_shared_from_this<Session<T>> {

    public:
        Session(asio::io_context &context, SafeQueue<OwnedMessage<T>> &queueIn, asio::ip::udp::socket &&socket, int id) :
                socket_(std::move(socket)), context_(context), queueIn_(queueIn),
                stunSession_(STUN_HOST, STUN_PORT, context, socket_)  {
            id_ = id;
            is_writing = false;
        }

        virtual ~Session() = default;

        void send(const Message<T>& msg) {
            queueOut_.push_back(msg);
            asio::post(context_,
                       [this, msg] {
                           if (!is_writing) {
                               writeHeader();
                           }
                       }
            );
        }

        void disconnect() {
            std::cerr << "Disconnected\n";
            socket_.close();
        };

        void startStunSession() {
            asio::post(context_,
                       [this]() {
                           stunSession_.sendRequest();
                       }
            );
        }

        void asyncDisconnect() {
            if (isConnected())
                asio::post(context_, [this]{socket_.close();});
        }

        bool isConnected() const {
            return socket_.is_open();
        }

        void bindToLocalEndpoint(asio::ip::udp::endpoint &ep) {
            socket_.bind(ep);
        }

        void connectWithEndpoint(asio::ip::udp::endpoint ep, T pingType) {
            stunSession_.stop();
            socket_.async_connect(ep,
                                  [this, pingType] (std::error_code ec) {
                                      if (!ec) {
                                          startListening();
                                          ping(pingType);
                                      } else {
                                          std::cerr << ec.message() << "\n";
                                      }
                                  }
            );
        }

        asio::ip::udp::endpoint getRealEp() {
            return stunSession_.getEndpoint();
        }

        void ping(T pingType) {
            Message<T> msg;
            msg.header_.id_ = pingType;
            send(msg);
        }

        void startListening() {
            asio::post(context_,
                       [this] () {
                           readHeader();
                       }
            );
        }

        asio::basic_socket<asio::ip::udp>::endpoint_type getEndpoint() {
            return socket_.remote_endpoint();
        }

        uint16_t getId() {
            return id_;
        }

    private:

        void writeHeader() {
            is_writing = true;
            tempMsgOut_ = std::move(queueOut_.pop_back());
            socket_.async_send(asio::buffer(&tempMsgOut_.header_, sizeof(MessageHeader<T>)),
                               [this] (std::error_code er, size_t length) {
                                   if (!er) {
                                       if (tempMsgOut_.body_.size() > 0) {
                                           writeBody();
                                       }
                                       else {
                                           if (queueOut_.empty())
                                               is_writing = false;
                                           else
                                               writeHeader();
                                       }
                                   }
                                   else {
                                       disconnect();
                                   }
                               }
            );
        }

        void writeBody() {
            socket_.async_send(asio::buffer(tempMsgOut_.body_.data(), tempMsgOut_.body_.size()),
                               [this] (std::error_code er, size_t length) {
                                   if (!er) {
                                       if (!queueOut_.empty())
                                           writeHeader();
                                       else
                                           is_writing = false;
                                   }
                                   else {
                                       disconnect();
                                   }
                               }
            );
        }

        void readHeader() {
            socket_.async_receive(asio::buffer(&tempMsgIn_.header_, sizeof(MessageHeader<T>)),
                                  [this] (std::error_code er, size_t length) {
                                      if (!er) {
                                          if (tempMsgIn_.header_.size_ > 0) {
                                              tempMsgIn_.body_.resize(tempMsgIn_.header_.size_);
                                              readBody();
                                          }
                                          else {
                                              addToQueue();
                                          }
                                      }
                                      else {
                                          disconnect();
                                      }
                                  }
            );
        }

        void readBody() {
            socket_.async_receive(asio::buffer(tempMsgIn_.body_.data(), tempMsgIn_.body_.size()),
                                  [this] (std::error_code er, size_t length) {
                                      if (!er) {
                                          addToQueue();
                                      }
                                      else {
                                          disconnect();
                                      }
                                  }
            );
        }

        void addToQueue() {
            queueIn_.push_back({this->shared_from_this(), tempMsgIn_});
            tempMsgIn_.clear();
            readHeader();
        }

    private:
        asio::ip::udp::socket socket_;
        asio::io_context &context_;
        StunSession stunSession_;
        SafeQueue<Message<T>> queueOut_;
        Message<T> tempMsgOut_;
        SafeQueue<OwnedMessage<T>> &queueIn_;
        Message<T> tempMsgIn_;
        std::mutex mutex_;
        bool is_writing;
        uint16_t id_;
    };
}
