#pragma once

#include "netlib_server.h"
#include "netlib_header.h"
#include "netlib_typesenum.h"
#include "netlib_nodeserver.h"

namespace netlib {

    void serverUpdateCircle(NodeServer &server, bool &isRunning) {
        while (isRunning) {
            server.updateNode();
        }
    }

    class Node {

    public:
        Node(const std::string& localAddress, uint16_t port, std::string downloadPath):
        m_server(localAddress, port, std::move(downloadPath)){
            m_isRunning = true;
            m_server.start();
            m_updateThread = std::thread(netlib::serverUpdateCircle, std::ref(m_server), std::ref(m_isRunning));
        }

        void stop() {
            m_isRunning = false;
            if (m_updateThread.joinable())
                m_updateThread.join();
            m_server.stop();
        }

        uint16_t addDirectConnection(asio::ip::udp::endpoint &ep) {
            return m_server.connect(ep);
        }

        uint16_t addDirectConnection(uint64_t inviteCode) {
            auto ep = epFromCode(inviteCode);
            return addDirectConnection(ep);
        }

        uint64_t getInviteCode() {
            auto realEp = m_server.getRealEp();
            std:: cout << realEp.address() << " " << realEp.port() << "\n";
            return ((uint64_t)(realEp.address().to_v4().to_ulong()) << 16) + realEp.port();
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
            m_server.downloadFile(path);
        }

    private:
        NodeServer m_server;

        bool m_isRunning;
        std::thread m_updateThread;
    };
}
