#pragma once

#include "netlib_header.h"
#include "netlib_session.h"
#include "netlib_message.h"
#include "netlib_safequeue.h"
#include "netlib_session.h"

namespace netlib {

    template<typename T>
    class Server {
    public:

        Server(std::string localAddress, uint16_t port, T pingType, T pongType) {
            m_curId = port;
            m_context = new asio::io_context();
            m_pingType = pingType;
            m_pongType = pongType;
            m_localAddress = std::move(localAddress);
        }

        ~Server() = default;

        void start() {
            m_idleWork = new asio::io_context::work(*m_context);
            m_contextThread = std::thread([this]() {m_context->run();});
            std::cout << "[SERVER] Started" << "\n";
            prepareSession();
        }

        void stop() {
            m_context->stop();
            if (m_contextThread.joinable()) m_contextThread.join();
            delete m_idleWork;

            delete m_context;
            
            std::cout << "[SERVER] Stopped\n";
        }

        void prepareSession() {
            if (m_sessionsMap.find(m_curId) != m_sessionsMap.end())
                return;
            std::shared_ptr<Session<T>> newSes =
                    std::make_shared<Session<T>>(m_context, m_queueIn, asio::ip::udp::socket(*m_context, asio::ip::udp::v4()), m_curId, m_pongType);
            asio::ip::udp::endpoint localEndpoint = asio::ip::udp::endpoint(asio::ip::address_v4::from_string(m_localAddress), m_curId);
            newSes->bindToLocalEndpoint(localEndpoint);
            newSes->startStunSession();
            m_sessionsMap[m_curId] = newSes;
        }

        uint16_t connectToHost(asio::ip::udp::endpoint &ep) {
            std::string host = ep.address().to_string();
            return connectToHost(host, ep.port());
        }

        uint16_t reservePort() {
            m_curId++;
            prepareSession();
            return m_curId - 1;
        }

        uint16_t connectToHost(std::string &host, uint16_t port) {
            try {
                connectToHost(host, port, m_curId);
                m_curId++;
                prepareSession();
                return m_curId - 1;
            }
            catch (std::exception &ex) {
                std::cerr << ex.what() << "\n";
                return -1;
            }
        }

        uint16_t connectToHost(std::string host, uint16_t port, uint16_t id) {
            asio::ip::udp::resolver resolver(*m_context);
            asio::ip::udp::resolver::query query(host, std::to_string(port));
            asio::ip::udp::resolver::iterator iter = resolver.resolve(query);
            if (m_sessionsMap.find(m_curId) == m_sessionsMap.end())
                prepareSession();
            m_sessionsMap[id]->connectWithEndpoint(*iter, m_pingType);
            std::cout << "[SERVER]: New UDP connection to " << (*iter).endpoint() << " created on port " << id << "\n";
            return id;
        }

        uint16_t connectToHost(asio::ip::udp::endpoint &ep, uint16_t id) {

            return connectToHost(ep.address().to_string(), ep.port(), id);
        }

        void disconnectClient(uint16_t id) {
            if (m_sessionsMap.find(id) == m_sessionsMap.end())
                return;
            std::shared_ptr<Session<T>> client = m_sessionsMap[id];
            client->asyncDisconnect();
            m_sessionsMap.erase(id);
        }

        void sendMessage(uint16_t id, Message<T> &msg) {
            if (m_sessionsMap.find(id) == m_sessionsMap.end()) {
                std::cout << "There is no such port!\n";
                return;
            }
            std::shared_ptr<Session<T>> client = m_sessionsMap[id];
            if (client && client->isConnected()) {
                client->send(msg);
            }
            else {
                disconnectClient(id);
            }
        }

        void update() {
            while (!m_queueIn.empty()) {
                OwnedMessage<T> msg = m_queueIn.pop_front();
                onMessage(msg);
            }
        }


        asio::ip::udp::endpoint getRealEp() {
            if (m_sessionsMap.find(m_curId) == m_sessionsMap.end())
                prepareSession();
            return m_sessionsMap[m_curId]->getRealEp();
        }


    protected:

        virtual void onMessage(OwnedMessage<T> &msg) {

        };

        virtual void onDisconnect(std::shared_ptr<Session<T>> session) {

        };

    protected:
        asio::io_context *m_context;
        asio::io_context::work *m_idleWork;
        std::thread m_contextThread;

        SafeQueue<OwnedMessage<T>> m_queueIn;
        std::map<uint16_t, std::shared_ptr<Session<T>>> m_sessionsMap;

        uint16_t m_curId = 999;
        std::string m_localAddress = "0.0.0.0";

        SafeQueue<Message<T>> m_eventQueue;

        T m_pingType, m_pongType;
    };
}