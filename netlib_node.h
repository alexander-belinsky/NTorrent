#pragma once

#include "netlib_server.h"
#include "netlib_header.h"
#include "netlib_typesenum.h"
#include "netlib_nodeserver.h"

namespace netlib {

    class Node {

    public:

        static void serverUpdateCircle(NodeServer &server, bool &isRunning, asio::ip::udp::endpoint &ep, std::mutex &mutex) {
            while (isRunning) {
                std::scoped_lock lock(mutex);
                ep = server.updateNode();
            }
        }

        Node(const std::string& localAddress, uint16_t port, std::string downloadPath):
        m_server(localAddress, port, std::move(downloadPath)){
            m_isRunning = true;
            m_server.start();
            m_updateThread = std::thread(netlib::Node::serverUpdateCircle, std::ref(m_server), std::ref(m_isRunning),
                                         std::ref(m_curEp), std::ref(mutex));
        }

        void stop() {
            m_isRunning = false;
            if (m_updateThread.joinable())
                m_updateThread.join();
            m_server.stop();
        }

        uint16_t addDirectConnection(asio::ip::udp::endpoint &ep) {
            std::scoped_lock lock(mutex);
            return m_server.connect(ep);
        }

        uint16_t addDirectConnection(uint64_t inviteCode) {
            auto ep = epFromCode(inviteCode);
            return addDirectConnection(ep);
        }

        uint64_t getInviteCode() {
            std:: cout << m_curEp.address() << " " << m_curEp.port() << "\n";
            return ((uint64_t)(m_curEp.address().to_v4().to_ulong()) << 16) + m_curEp.port();
        }

        static asio::ip::udp::endpoint epFromCode(uint64_t code) {
            asio::ip::udp::endpoint endpoint(asio::ip::address_v4(code >> 16), code % (1 << 16));
            std::cout << endpoint.address() << " " << endpoint.port() << "\n";
            return endpoint;
        }

        void uploadFile(std::string &path) {
            m_server.uploadFile(path);
        }

        void downloadFile(std::string &path) {
            std::scoped_lock lock(mutex);
            m_server.downloadFile(path);
        }

    private:
        NodeServer m_server;

        bool m_isRunning;
        std::thread m_updateThread;

        asio::ip::udp::endpoint m_curEp;
        std::mutex mutex;
    };
}
