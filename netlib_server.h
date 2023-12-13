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

        Server(uint16_t port) {
            curId_ = port;
        }

        ~Server() {
            stop();
        }

        void start() {
            idleWork_ = new asio::io_context::work(context_);
            contextThread_ = std::thread([this]() {context_.run();});
            std::cout << "[SERVER] Started" << "\n";
            prepareSession();
        }

        void stop() {
            context_.stop();
            delete idleWork_;
            if (contextThread_.joinable()) contextThread_.join();
            std::cout << "[SERVER] Stopped\n";
        }

        void prepareSession() {
            if (sessionsMap_.find(curId_) != sessionsMap_.end())
                return;
            std::shared_ptr<Session<T>> newSes =
                    std::make_shared<Session<T>>(context_, queueIn_, asio::ip::udp::socket(context_, asio::ip::udp::v4()), curId_);
            asio::ip::udp::endpoint localEndpoint = asio::ip::udp::endpoint(asio::ip::address_v4::from_string(localAddress_), curId_);
            newSes->bindToLocalEndpoint(localEndpoint);
            newSes->startStunSession();
            sessionsMap_[curId_] = newSes;
        }

        void connectToHost(std::string &host, uint16_t port, T pingType) {
            try {
                asio::ip::udp::resolver resolver(context_);
                asio::ip::udp::resolver::query query(host, std::to_string(port));
                asio::ip::udp::resolver::iterator iter = resolver.resolve(query);
                if (sessionsMap_.find(curId_) == sessionsMap_.end())
                    prepareSession();
                sessionsMap_[curId_]->connectWithEndpoint(*iter, pingType);
                std::cout << "[SERVER]: New UDP connection to " << (*iter).endpoint() << " created on port " << curId_ << "\n";
                curId_++;
                prepareSession();
            }
            catch (std::exception &ex) {
                std::cerr << ex.what() << "\n";
            }
        }

        void sendMessage(uint16_t id, Message<T> &msg) {
            if (sessionsMap_.find(id) == sessionsMap_.end()) {
                std::cout << "There is no such port!\n";
                return;
            }
            std::shared_ptr<Session<T>> client = sessionsMap_[id];
            if (client && client->isConnected()) {
                client->send(msg);
            }
            else {
                onDisconnect(client);
                client.reset();
                sessionsMap_.erase(id);
            }
        }

        void update() {
            while (!queueIn_.empty()) {
                OwnedMessage<T> msg = queueIn_.pop_back();
                onMessage(msg);
            }
        }

        asio::ip::udp::endpoint getRealEp() {
            if (sessionsMap_.find(curId_) == sessionsMap_.end())
                prepareSession();
            return sessionsMap_[curId_]->getRealEp();
        }

    protected:

        virtual void onMessage(OwnedMessage<T> &msg) {

        };

        virtual void onDisconnect(std::shared_ptr<Session<T>> session) {

        };

    protected:
        asio::io_context context_;
        asio::io_context::work* idleWork_;
        std::thread contextThread_;
        SafeQueue<OwnedMessage<T>> queueIn_;
        std::map<uint16_t, std::shared_ptr<Session<T>>> sessionsMap_;
        uint16_t curId_ = 999;
        std::string localAddress_ = "0.0.0.0";
    };
}