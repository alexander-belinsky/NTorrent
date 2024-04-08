#pragma once
#include "netlib_header.h"
#include "netlib_message.h"
#include "netlib_safequeue.h"
#include "netlib_natkiller.h"

std::string STUN_HOST = "stun.l.google.com";
uint32_t STUN_PORT = 19302;

const long MAX_TIME_CONNECTING = 20;
const long MAX_TIME_DISABLE = 10;

namespace netlib {
    template<typename T>
    class Session : public std::enable_shared_from_this<Session<T>> {

    public:
        Session(asio::io_context *context, SafeQueue<OwnedMessage<T>> &queueIn, asio::ip::udp::socket &&socket, int id, T pongType) :
                m_socket(std::move(socket)), m_context(context), m_queueIn(queueIn),
                m_stunServer(STUN_HOST, STUN_PORT, context, m_socket), m_pongType(pongType), m_timer(*m_context)  {
            m_id = id;
            is_writing = false;
            is_waiting = false;
            m_lastClock = clock() + CLOCKS_PER_SEC * MAX_TIME_CONNECTING;
        }

        virtual ~Session() = default;

        void sendNoAnswer(Message<T> &msg) {
            if (!m_isConnected)
                return;
            msg << false;
            m_queueOut.push_back(msg);
            if (!is_writing && !is_waiting) {
                writeHeader();
            }
            bool f;
            msg >> f;
        }

        void send(Message<T> &msg) {
            if (!m_isConnected);
            msg << true;
            m_queueOut.push_back(msg);
            if (!is_writing && !is_waiting) {
                writeHeader();
            }
            bool f;
            msg >> f;
        }

        void disconnect() {
            std::cerr << "Disconnected\n";
            m_socket.close();
        };

        void startStunSession() {
            asio::post(*m_context,
                       [this]() {
                           m_stunServer.sendRequest();
                       }
            );
        }

        void asyncDisconnect() {
            if (isConnected())
                asio::post(*m_context, [this]{m_socket.close();});
        }

        bool isConnected() const {
            return m_socket.is_open();
        }

        void bindToLocalEndpoint(asio::ip::udp::endpoint &ep) {
            m_socket.bind(ep);
        }

        void connectWithEndpoint(asio::ip::udp::endpoint ep, T pingType) {
            m_stunServer.stop();

            m_socket.async_connect(ep,
                                   [this, pingType] (std::error_code ec) {
                                      if (!ec) {
                                          m_isConnected = true;
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
            return m_stunServer.getEndpoint();
        }

        void ping(T pingType) {
            m_pingType = pingType;
            Message<T> msg;
            msg.m_header.id = pingType;
            sendNoAnswer(msg);
        }

        void startListening() {
            asio::post(*m_context,
                       [this] () {
                           readHeader();
                       }
            );
        }

        asio::basic_socket<asio::ip::udp>::endpoint_type getEndpoint() {
            return m_socket.remote_endpoint();
        }

        uint16_t getId() {
            return m_id;
        }

        bool checkAble() {
            clock_t curClock = clock();
            if (curClock - m_lastClock >= MAX_TIME_DISABLE * CLOCKS_PER_SEC && m_isConnected) {
                return false;
            }
            return true;
        }

    private:

        void endOfWriting() {
            bool shouldWait;
            m_tempMsgOut >> shouldWait;
            if (shouldWait) {
                is_waiting = true;
                is_writing = false;
            } else {
                if (m_queueOut.empty())
                    is_writing = false;
                else
                    writeHeader();
            }
        }

        void endOfReading() {
            bool shouldAnswer;
            m_tempMsgIn >> shouldAnswer;
            m_lastClock = clock();
            if (m_tempMsgIn.m_header.id == m_pongType) {
                is_waiting = false;
                if (!is_writing && !m_queueOut.empty())
                    writeHeader();
            }
            else {
                if (shouldAnswer) {
                    Message<T> msg;
                    msg.m_header.id = m_pongType;
                    sendNoAnswer(msg);
                }
                m_queueIn.push_back({this->shared_from_this(), m_tempMsgIn});
            }
            m_tempMsgIn.clear();
            readHeader();

        }

        void writeHeader() {
            is_writing = true;
            m_tempMsgOut = m_queueOut.pop_front();
            m_socket.async_send(asio::buffer(&m_tempMsgOut.m_header, sizeof(MessageHeader<T>)),
                                [this] (std::error_code er, size_t length) {
                       if (!er) {
                           if (m_tempMsgOut.m_body.size() > 0) {
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
            m_socket.async_send(asio::buffer(m_tempMsgOut.m_body.data(), m_tempMsgOut.m_body.size()),
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
            m_socket.async_receive(asio::buffer(&m_tempMsgIn.m_header, sizeof(MessageHeader<T>)),
                                   [this] (std::error_code er, size_t length) {
                  if (!er) {
                      if (m_tempMsgIn.m_header.size > 0) {
                          m_tempMsgIn.m_body.resize(m_tempMsgIn.m_header.size);
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
            m_socket.async_receive(asio::buffer(m_tempMsgIn.m_body.data(), m_tempMsgIn.m_body.size()),
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
            m_timer.expires_after(std::chrono::milliseconds(3000));
            m_timer.async_wait(
                [this](std::error_code ec) {
                    if (!ec) {
                        if (m_queueOut.empty())
                            ping(m_pingType);
                        if (m_isConnected)
                            m_lastClock = clock();
                        pingCycle();
                    } else {
                        std::cerr << "Waiting error: " << ec.message() << "\n";
                    }
                }
            );
        };

    private:
        asio::ip::udp::socket m_socket;
        asio::io_context *m_context;
        StunSession m_stunServer;
        SafeQueue<Message<T>> m_queueOut;
        Message<T> m_tempMsgOut;
        SafeQueue<OwnedMessage<T>> &m_queueIn;
        Message<T> m_tempMsgIn;
        std::mutex m_mutex;
        bool is_writing = false;
        bool is_waiting = false;

        uint16_t m_id;

        T m_pongType, m_pingType;

        asio::steady_timer m_timer;

        clock_t m_lastClock = 0;

        bool m_isConnected = false;
    };
}
