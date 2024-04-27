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
            stunSession_.start();
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

        void pong(uint64_t msgId) {
            Message<T> msg(pongType_);
            msg << msgId;
            sendNoAnswer(msg);
            shouldPong = false;
        }

        void tryPong(uint64_t msgId) {
            if (!is_writing) {
                pong(std::max(msgId, m_maxPong));
                return;
            }
            m_maxPong = std::max(msgId, m_maxPong);
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
            uint64_t msgId;
            tempMsgOut_ >> msgId;
            tempMsgOut_ << msgId;
            tempMsgOut_ << shouldWait;
            //std::cout << "endOfWriting " << (uint16_t)tempMsgOut_.m_header.id << " " << msgId << "\n";
            is_writing = false;
            if (shouldPong)
                pong(m_maxPong);
            if (shouldWait) {
                if (msgId == lastSentId + 1) {
                    lastSentId++;
                    m_latestMsg.push_back({tempMsgOut_, (uint64_t)clock()});
                }
            } else {
                if (shouldPing)
                    ping(pingType_);
                if (!queueOut_.empty() && m_isConnected)
                    writeHeader();
            }
        }

        void endOfReading() {
            bool shouldAnswer;
            tempMsgIn_ >> shouldAnswer;
            //std::cout << "endOfReading " << (uint16_t)tempMsgIn_.m_header.id << " " << tempMsgIn_.m_header.size << "\n";
            m_lastClock = clock();
            if (!m_isConnected) {
                checkRepeat();
            }
            m_isConnected = true;
            if (tempMsgIn_.m_header.id == pongType_) {
                uint64_t pongMsgId;
                tempMsgIn_ >> pongMsgId;
                //std::cout << "pongId: " << pongMsgId << "\n";
                while (pongMsgId + m_latestMsg.size() > lastSentId) {
                    m_latestMsg.pop_front();
                }
            }
            else {
                if (tempMsgIn_.m_header.id != pingType_) {
                    uint64_t msgId;
                    tempMsgIn_ >> msgId;
                    //std::cout << "msgId: " << msgId << "\n";
                    if (shouldAnswer && msgId <= lastGotId + 1) {
                        tryPong(msgId);
                    }
                    if (msgId >= lastGotId + 1) {
                        m_futureMsgMap[msgId] = tempMsgIn_;
                    } else {
                        //std::cout << "! skipped message\n";
                    }
                    uint16_t delta = 0;
                    while (m_futureMsgMap.find(lastGotId + 1) != m_futureMsgMap.end()) {
                        queueIn_.push_back({this->shared_from_this(), m_futureMsgMap[lastGotId + 1]});
                        m_futureMsgMap.erase(lastGotId + 1);
                        lastGotId++;
                        delta++;
                    }
                    if (delta > 1)
                        tryPong(lastGotId);
                }
            }
            tempMsgIn_.clear();
            if (!is_writing) {
                if (shouldPong)
                    pong(m_maxPong);
            }
            if (!is_writing && !queueOut_.empty())
                writeHeader();
            readHeader();
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
            //std::cout << "writeBody " << (uint16_t )tempMsgOut_.m_header.id << " " << tempMsgOut_.m_body.size() << "\n";
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
                                              //std::cout << length << " <- not ness " << (uint16_t)tempMsgIn_.m_header.id << "\n";
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

        void checkRepeat() {
            repeatTimer.expires_after(std::chrono::milliseconds(MAX_NO_ANSWER));
            repeatTimer.async_wait(
                    [this](std::error_code ec) {
                        if (m_isConnected && !m_latestMsg.empty() && clock() > MAX_NO_ANSWER + m_latestMsg.front().time) {
                            m_latestMsg.front().time = clock();
                            queueOut_.push_front(m_latestMsg.front().msg);
                        }
                        checkRepeat();
            });
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
                            if (!is_writing)
                                checkRepeat();
                            pingCycle();
                        } else {
                            std::cerr << "Waiting error: " << ec.message() << "\n";
                        }
                    }
            );
        };

        struct MemMsg {
            Message<T> msg;
            uint64_t time;
            MemMsg(Message<T> m, uint64_t t): msg(m){
                time = t;
            }
        };

    private:
        asio::ip::udp::socket socket_;
        asio::io_context *context_;
        StunSession stunSession_;
        SafeQueue<Message<T>> queueOut_;
        Message<T> tempMsgOut_;
        SafeQueue<OwnedMessage<T>> &queueIn_;
        Message<T> tempMsgIn_;

        SafeQueue<MemMsg> m_latestMsg;
        uint64_t lastSentId = 0;
        uint64_t lastGotId = 0;
        uint64_t currentId = 0;

        std::mutex mutex_;
        bool is_writing = false;

        bool shouldPong = false;
        uint64_t m_maxPong = 0;
        bool shouldPing = false;

        uint16_t id_;

        T pongType_, pingType_;

        asio::steady_timer timer;
        asio::steady_timer repeatTimer;

        bool m_isConnected = false;
        std::clock_t m_lastClock = 0;

        asio::ip::udp::endpoint remoteEp_;
        asio::ip::udp::endpoint tempEp_;

        std::map<uint16_t, Message<T>> m_futureMsgMap;
    };
}