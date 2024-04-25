#pragma once
#include "netlib_header.h"
#include "netlib_message.h"
#include "netlib_safequeue.h"
#include "netlib_natkiller.h"

std::string STUN_HOST = "stun.l.google.com";
uint32_t STUN_PORT = 19302;

uint16_t MAX_TIME_OUT = 20;
uint16_t MAX_NO_ANSWER = 100;

uint32_t MAX_PACKET_SIZE = 1 << 15;

uint16_t MAX_PACKET_ID = 17;

namespace netlib {

    template<typename T>
    class Session : public std::enable_shared_from_this<Session<T>> {

    public:
        Session(asio::io_context *context, SafeQueue<OwnedMessage<T>> &queueIn, asio::ip::udp::socket &&socket, int id, T pongType) :
                socket_(std::move(socket)), context_(context), queueIn_(queueIn),
                stunSession_(STUN_HOST, STUN_PORT, context, socket_), pongType_(pongType), timer(*context_),
                repeatTimer(*context)  {
            id_ = id;
            is_writing = false;

            m_lastClock = clock();
        }

        virtual ~Session() = default;

        enum class SessionType {
            DefaultSession,
            FileSession,
        };
        SessionType m_type = SessionType::DefaultSession;

        void sendNoAnswer(Message<T> &msg) {
            msg << false;
            //std::cout << "sendNoAnswer " << (uint16_t) msg.m_header.id << " " << m_isConnected << " " << getId() << "\n";
            socket_.send(asio::buffer(&msg.m_header, sizeof(MessageHeader<T>)));
            socket_.send(asio::buffer(msg.m_body.data(), msg.m_body.size()));
            bool f;
            msg >> f;
        }

        void send(Message<T> &msg) {
            currentId++;
            msg << currentId;
            msg << true;
            queueOut_.push_back(msg);
            if (!is_writing && m_isConnected) {
                m_lastPongResp = clock();
                writeHeader();
            }
            bool f;
            msg >> f;
            msg >> currentId;
        }

        void disconnect() {
            if (!socket_.is_open())
                return;
            std::cerr << "Disconnected\n";
            socket_.close();
        };

        void startStunSession() {
            asio::post(*context_,
                       [this]() {
                           stunSession_.start();
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
            if (!is_writing) {
                pingType_ = pingType;
                Message<T> msg(pingType_);
                sendNoAnswer(msg);
                shouldPing = false;
                return;
            }
            shouldPing = true;
        }

        void pong(uint16_t msgId) {
            Message<T> msg(pongType_);
            msg << msgId;
            sendNoAnswer(msg);
        }

        void tryPong(uint16_t msgId) {
            std::cout << "tryPong\n";
            if (!is_writing) {
                pong(msgId);
                return;
            }
            m_pongIds.push_back(msgId);
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
            uint16_t msgId;
            tempMsgOut_ >> msgId;
            tempMsgOut_ << msgId;
            tempMsgOut_ << shouldWait;
            std::cout << "endOfWriting " << (uint16_t)tempMsgOut_.m_header.id << " " << msgId << "\n";
            is_writing = false;
            while (!m_pongIds.empty()) {
                pong(m_pongIds.pop_front());
            }
            m_pongIds.clear();
            if (shouldWait) {
                if (msgId == lastSentId + 1) {
                    lastSentId++;
                    m_latestMsg.push_back(tempMsgOut_);
                }
                checkRepeat();
            } else {
                if (shouldPing)
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
            std::cout << "endOfReading " << (uint16_t)tempMsgIn_.m_header.id << " " << tempMsgIn_.m_header.size << "\n";
            m_lastClock = clock();
            if (!m_isConnected)
                m_lastPongResp = clock();
            m_isConnected = true;
            if (tempMsgIn_.m_header.id == pongType_) {
                uint16_t pongMsgId;
                tempMsgIn_ >> pongMsgId;
                std::cout << "pongId: " << pongMsgId << "\n";
                if (pongMsgId >= lastSentId - m_latestMsg.size())
                    m_lastPongResp = clock();
                for (int i = pongMsgId; i + m_latestMsg.size() <= lastSentId; i++) {
                    m_latestMsg.pop_front();
                }
            }
            else {
                if (tempMsgIn_.m_header.id != pingType_) {
                    uint16_t msgId;
                    tempMsgIn_ >> msgId;
                    if (shouldAnswer && msgId <= lastGotId + 1)
                        tryPong(msgId);
                    if (msgId == lastGotId + 1) {
                        queueIn_.push_back({this->shared_from_this(), tempMsgIn_});
                        lastGotId++;
                    }
                }
            }
            tempMsgIn_.clear();
            readHeader();
            if (!is_writing && !queueOut_.empty())
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
                                       std::cout << "writeHeader error:" << er.message() << "\n";
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
                                       std::cout << "writeBody error:" << er.message() << "\n";
                                       disconnect();
                                   }
                               }
            );
        }

        void readHeader() {
            socket_.async_receive_from(asio::buffer(&tempMsgIn_.m_header, sizeof(MessageHeader<T>)), tempEp_,
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
                                          std::cout << "readHeader " << (uint16_t)tempMsgIn_.m_header.id << " " << tempMsgIn_.m_header.size << "\n";
                                          if (tempMsgIn_.m_header.size > 0) {
                                              tempMsgIn_.m_body.resize(tempMsgIn_.m_header.size);
                                              readBody();
                                          }
                                          else {
                                              endOfReading();
                                          }
                                      }
                                      else {
                                          std::cout << "readHeader error:" << er.message() << "\n";
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
                                          std::cout << "readBody error:" << er.message() << "\n";
                                          disconnect();
                                      }
                                  }
            );
        }

        void checkRepeat() {
            if (m_isConnected && (clock() - m_lastPongResp > MAX_NO_ANSWER) && !m_latestMsg.empty()) {
                //std::cout << "checkRepeat::repeat\n";
                m_lastPongResp = clock();
                queueOut_.push_front(m_latestMsg.front());
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
                            if (queueOut_.empty() || !m_isConnected)
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

        SafeQueue<Message<T>> m_latestMsg;
        uint16_t lastSentId = 0;
        uint16_t lastGotId = 0;
        uint16_t currentId = 0;

        SafeQueue<uint16_t> m_pongIds;

        std::mutex mutex_;
        bool is_writing = false;

        bool shouldPing = false;

        uint16_t id_;

        T pongType_, pingType_;

        asio::steady_timer timer;
        asio::steady_timer repeatTimer;

        bool m_isConnected = false;
        std::clock_t m_lastClock = 0;
        std::clock_t m_lastPongResp = 0;

        asio::ip::udp::endpoint remoteEp_;
        asio::ip::udp::endpoint tempEp_;
    };
}