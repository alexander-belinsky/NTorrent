#pragma once
#include "netlib_header.h"
#include "netlib_message.h"
#include "netlib_safequeue.h"
#include "netlib_natkiller.h"

std::string STUN_HOST = "stun.l.google.com";
uint32_t STUN_PORT = 19302;

uint16_t MAX_TIME_OUT = 20;

namespace netlib {
    template<typename T>
    class Session : public std::enable_shared_from_this<Session<T>> {

    public:
        Session(asio::io_context *context, SafeQueue<OwnedMessage<T>> &queueIn, asio::ip::udp::socket &&socket, int id, T pongType) :
                socket_(std::move(socket)), context_(context), queueIn_(queueIn),
                stunSession_(STUN_HOST, STUN_PORT, context, socket_), pongType_(pongType), timer(*context_)  {
            id_ = id;
            is_writing = false;
            is_waiting = false;

            m_lastClock = clock();
        }

        virtual ~Session() = default;

        void sendNoAnswer(Message<T> &msg) {
            msg << false;
            std::cout << "sendNoAnswer " << (uint16_t) msg.m_header.id << "\n";
            socket_.send(asio::buffer(&msg.m_header, sizeof(MessageHeader<T>)));
            socket_.send(asio::buffer(msg.m_body.data(), msg.m_body.size()));
            bool f;
            msg >> f;
        }

        void send(Message<T> &msg) {
            msg << true;
            queueOut_.push_back(msg);
            if (!is_writing && !is_waiting && m_isConnected) {
                writeHeader();
            }
            bool f;
            msg >> f;
        }

        void disconnect() {
            std::cerr << "Disconnected\n";
            socket_.close();
        };

        void startStunSession() {
            asio::post(*context_,
                       [this]() {
                           stunSession_.sendRequest();
                       }
            );
        }

        void asyncDisconnect() {
            if (isConnected())
                asio::post(*context_, [this]{socket_.close();});
        }

        bool isConnected() const {
            return socket_.is_open();
        }

        void bindToLocalEndpoint(asio::ip::udp::endpoint &ep) {
            socket_.bind(ep);
        }

        void connectionWait(asio::ip::udp::endpoint &ep) {

        }

        void async_read(asio::const_buffer &buffer, const std::function<void()> func, size_t curInd = 0) {
            if (curInd >= buffer.size()) {
                func();
                return;
            }
            socket_.async_receive(asio::buffer(IntToPtr(PtrToInt(buffer.data()) + curInd), buffer.size() - curInd),
                                  [this, buffer, func, curInd](std::error_code er, size_t length){
                                      async_read(buffer, func, curInd + length);
            });
        }

        void connectWithEndpoint(asio::ip::udp::endpoint ep, T pingType) {
            stunSession_.stop();
            socket_.async_connect(ep,
                  [this, pingType] (std::error_code ec) {
                      if (!ec) {
                          startListening();
                          ping(pingType);
                          pingCycle();
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
            pingType_ = pingType;
            Message<T> msg;
            msg.m_header.id = pingType;
            sendNoAnswer(msg);
        }

        void startListening() {
            asio::post(*context_,
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

        bool checkAble() {
            if (m_isConnected && clock() - m_lastClock > MAX_TIME_OUT * CLOCKS_PER_SEC)
                return false;
            return true;
        }

    private:

        void endOfWriting() {
            bool shouldWait;
            tempMsgOut_ >> shouldWait;
            std::cout << "endOfWriting " << (uint16_t)tempMsgOut_.m_header.id << "\n";
            if (shouldWait) {
                is_waiting = true;
                is_writing = false;
            } else {
                if (queueOut_.empty() || !m_isConnected)
                    is_writing = false;
                else
                    writeHeader();
            }
        }

        void endOfReading() {
            bool shouldAnswer;
            tempMsgIn_ >> shouldAnswer;
            std::cout << "endOfReading " << (uint16_t)tempMsgIn_.m_header.id << "\n";
            m_lastClock = clock();
            m_isConnected = true;
            if (tempMsgIn_.m_header.id == pongType_) {
                is_waiting = false;
                if (!is_writing && !queueOut_.empty())
                    writeHeader();
            }
            else {
                if (shouldAnswer) {
                    Message<T> msg;
                    msg.m_header.id = pongType_;
                    sendNoAnswer(msg);
                }
                queueIn_.push_back({this->shared_from_this(), tempMsgIn_});
            }
            tempMsgIn_.clear();
            readHeader();
            if (!is_writing && isConnected() && !is_waiting && !queueOut_.empty())
                writeHeader();
        }

        void writeHeader() {
            is_writing = true;
            tempMsgOut_ = queueOut_.pop_front();
            std::cout << "writeHeader " << (uint16_t )tempMsgOut_.m_header.id << " " << tempMsgOut_.m_body.size() << "\n";
            socket_.async_send(asio::buffer(&tempMsgOut_.m_header, sizeof(MessageHeader<T>)),
                               [this] (std::error_code er, size_t length) {
                                   if (!er) {
                                       if (tempMsgOut_.m_body.size() > 0) {
                                           writeBody();
                                       }
                                       else {
                                           endOfWriting();
                                       }
                                   }
                                   else {
                                       disconnect();
                                   }
                               }
            );
        }

        void writeBody() {
            socket_.async_send(asio::buffer(tempMsgOut_.m_body.data(), tempMsgOut_.m_body.size()),
                               [this] (std::error_code er, size_t length) {
                                   if (!er) {
                                       endOfWriting();
                                   }
                                   else {
                                       disconnect();
                                   }
                               }
            );
        }

        void readHeader() {
            socket_.async_receive(asio::buffer(&tempMsgIn_.m_header, sizeof(MessageHeader<T>)),
                                  [this] (std::error_code er, size_t length) {
                                      if (!er) {
                                          if (tempMsgIn_.m_header.size > 0) {
                                              tempMsgIn_.m_body.resize(tempMsgIn_.m_header.size);
                                              readBody();
                                          }
                                          else {
                                              endOfReading();
                                          }
                                      }
                                      else {
                                          disconnect();
                                      }
                                  }
            );
        }

        void readBody() {
            socket_.async_receive(asio::buffer(tempMsgIn_.m_body.data(), tempMsgIn_.m_body.size()),
                                  [this] (std::error_code er, size_t length) {
                                      if (!er) {
                                          endOfReading();
                                      }
                                      else {
                                          disconnect();
                                      }
                                  }
            );
        }

        void pingCycle() {
            timer.expires_after(std::chrono::milliseconds(3000));
            timer.async_wait(
                    [this](std::error_code ec) {
                        if (!ec) {
                            if (queueOut_.empty())
                                ping(pingType_);
                            pingCycle();
                        } else {
                            std::cerr << "Waiting error: " << ec.message() << "\n";
                        }
                    }
            );
        };

    private:
        asio::ip::udp::socket socket_;
        asio::io_context *context_;
        StunSession stunSession_;
        SafeQueue<Message<T>> queueOut_;
        Message<T> tempMsgOut_;
        SafeQueue<OwnedMessage<T>> &queueIn_;
        Message<T> tempMsgIn_;
        std::mutex mutex_;
        bool is_writing = false;
        bool is_waiting = false;

        uint16_t id_;

        T pongType_, pingType_;

        asio::steady_timer timer;

        bool m_isConnected = false;
        std::clock_t m_lastClock = 0;
    };
}