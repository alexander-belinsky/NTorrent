#pragma once
#include "netlib_header.h"
#include "netlib_message.h"
#include "netlib_safequeue.h"
#include "netlib_natkiller.h"

std::string STUN_HOST = "stun.l.google.com";
uint32_t STUN_PORT = 19302;

uint16_t MAX_TIME_OUT = 20;
uint16_t MAX_NO_ANSWER = 200;

uint32_t MAX_PACKET_SIZE = 1 << 15;

uint16_t MAX_PACKET_ID = 17;

namespace netlib {
    template<typename T>
    class Session : public std::enable_shared_from_this<Session<T>> {

    public:
        Session(asio::io_context *context, SafeQueue<OwnedMessage<T>> &queueIn, asio::ip::udp::socket &&socket, int id, T pongType) :
                socket_(std::move(socket)), context_(context), queueIn_(queueIn),
                stunSession_(STUN_HOST, STUN_PORT, context, socket_), pongType_(pongType), timer(*context_),
                repeatTimer(*context), tcpSocket_(*context_, asio::ip::tcp::v4())  {
            id_ = id;
            is_writing = false;
            is_waiting = false;

            m_lastClock = clock();
        }

        virtual ~Session() = default;

        void sendNoAnswer(Message<T> &msg) {
            msg << false;
            //std::cout << "sendNoAnswer " << (uint16_t) msg.m_header.id << " " << m_isConnected << " " << getId() << "\n";
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
            return socket_.is_open() || tcpSocket_.is_open();
        }

        bool isActive() {
            return m_isConnected;
        }

        void bindToLocalEndpoint(asio::ip::udp::endpoint &ep) {
            socket_.bind(ep);
        }

        void connectWithEndpoint(asio::ip::udp::endpoint ep, T pingType) {
            pingType_ = pingType;
            stunSession_.stop();
            remoteEp_ = ep;
            socket_.async_connect(ep,
                  [this, pingType] (std::error_code ec) {
                      if (!ec) {
                          startListening();
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
            if (!is_writing && !is_waiting) {
                pingType_ = pingType;
                Message<T> msg(pingType_);
                sendNoAnswer(msg);
                shouldPing = false;
                return;
            }
            shouldPing = true;
        }

        void pong() {
            if (!is_writing) {
                Message<T> msg(pongType_);
                sendNoAnswer(msg);
                shouldPong = false;
                return;
            }
            shouldPong = true;
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
            if ((m_isConnected && clock() - m_lastClock > MAX_TIME_OUT * CLOCKS_PER_SEC) || !isConnected())
                return false;
            return true;
        }

    private:

        void endOfWriting() {
            bool shouldWait;
            tempMsgOut_ >> shouldWait;
            tempMsgOut_ << shouldWait;
            //std::cout << "endOfWriting " << (uint16_t)tempMsgOut_.m_header.id << "\n";
            is_writing = false;
            if (shouldPong)
                pong();
            if (shouldWait) {
                is_waiting = true;
                m_isWaitingClock = clock();
                repeatTimer.expires_after(std::chrono::milliseconds(MAX_NO_ANSWER + 10));
                repeatTimer.async_wait([this](std::error_code ec) {
                    repeatMessage();
                });
            } else {
                if (shouldPing && !is_waiting)
                    ping(pingType_);
                if (queueOut_.empty() || !m_isConnected)
                    is_writing = false;
                else
                    writeHeader();
            }
        }

        void endOfReading() {
            bool shouldAnswer;
            tempMsgIn_ >> shouldAnswer;
            //std::cout << "endOfReading " << (uint16_t)tempMsgIn_.m_header.id << " " << tempMsgIn_.m_header.size << "\n";
            m_lastClock = clock();
            if (!m_isConnected) {
                tcpSocketConnect();
            }
            m_isConnected = true;
            if (tempMsgIn_.m_header.id == pongType_) {
                is_waiting = false;
            }
            else {
                if (shouldAnswer) {
                    pong();
                }
                queueIn_.push_back({this->shared_from_this(), tempMsgIn_});
            }
            tempMsgIn_.clear();
            readHeader();
            if (!is_writing && !is_waiting && !queueOut_.empty())
                writeHeader();
        }

        void writeHeader() {
            is_writing = true;
            tempMsgOut_ = queueOut_.pop_front();
            //std::cout << "writeHeader " << (uint16_t )tempMsgOut_.m_header.id << " " << tempMsgOut_.m_body.size() << "\n";
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
            socket_.async_receive_from(asio::buffer(&tempMsgIn_.m_header, sizeof(MessageHeader<T>) + 1), tempEp_,
                                  [this] (std::error_code er, size_t length) {
                                      if (!er) {
                                          if (!m_isConnected && tempEp_.address() == remoteEp_.address())
                                              remoteEp_ = tempEp_;
                                          if (tempEp_ != remoteEp_) {
                                              readHeader();
                                              return;
                                          }
                                          if (length != sizeof(MessageHeader<T>) ||
                                                tempMsgIn_.m_header.size > MAX_PACKET_SIZE ||
                                                  (uint16_t)tempMsgIn_.m_header.id > MAX_PACKET_ID) {
                                              readHeader();
                                              return;
                                          }
                                          //std::cout << "readHeader " << (uint16_t)tempMsgIn_.m_header.id << " " << tempMsgIn_.m_header.size << "\n";
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
            socket_.async_receive_from(asio::buffer(tempMsgIn_.m_body.data(), tempMsgIn_.m_body.size()), tempEp_,
                                  [this] (std::error_code er, size_t length) {
                                      if (!er) {
                                          if (tempEp_ != remoteEp_) {
                                              readBody();
                                              return;
                                          }
                                          if (length != tempMsgIn_.m_body.size()) {
                                              std::cout << length << " <- not ness " << (uint16_t)tempMsgIn_.m_header.id << "\n";
                                              readHeader();
                                              return;
                                          }
                                          endOfReading();
                                      }
                                      else {
                                          disconnect();
                                      }
                                  }
            );
        }

        void repeatMessage() {
            //std::cout << "try repeat " << (uint16_t) tempMsgOut_.m_header.id << " " << m_isConnected << is_writing << is_waiting << " " << clock() - m_isWaitingClock << "\n";
            if (m_isConnected && !is_writing && is_waiting && (clock() - m_isWaitingClock > MAX_NO_ANSWER)) {
                queueOut_.push_front(tempMsgOut_);
                is_waiting = false;
                std::cout << "repeat " << (uint16_t) tempMsgOut_.m_header.id << "\n";
                writeHeader();
            }
        }

        void pingCycle() {
            timer.expires_after(std::chrono::milliseconds(3000));
            timer.async_wait(
                    [this](std::error_code ec) {
                        if (!ec) {
                            if (!checkAble()) {
                                disconnect();
                                return;
                            }
                            if (queueOut_.empty() || !m_isConnected && !is_waiting)
                                ping(pingType_);
                            pingCycle();
                        } else {
                            std::cerr << "Waiting error: " << ec.message() << "\n";
                        }
                    }
            );
        };

        void tcpSocketConnect() {
            is_writing = true;
            auto localEp = (asio::ip::basic_endpoint<asio::ip::tcp>)socket_.local_endpoint();
            socket_.close();
            tcpSocket_.bind(localEp);
            tcpSocket_.connect((asio::ip::basic_endpoint<asio::ip::tcp>) remoteEp_);
        }

    private:
        asio::ip::udp::socket socket_;
        asio::ip::tcp::socket tcpSocket_;
        asio::io_context *context_;
        StunSession stunSession_;
        SafeQueue<Message<T>> queueOut_;
        Message<T> tempMsgOut_;
        SafeQueue<OwnedMessage<T>> &queueIn_;
        Message<T> tempMsgIn_;
        std::mutex mutex_;
        bool is_writing = false;
        bool is_waiting = false;

        bool shouldPong = false;
        bool shouldPing = false;

        uint16_t id_;

        T pongType_, pingType_;

        asio::steady_timer timer;
        asio::steady_timer repeatTimer;

        bool m_isConnected = false;
        std::clock_t m_lastClock = 0;
        std::clock_t m_isWaitingClock = 0;

        asio::ip::udp::endpoint remoteEp_;
        asio::ip::udp::endpoint tempEp_;
    };
}