#pragma once
#include <utility>

#include "netlib_header.h"
#include "netlib_session.h"
#include "netlib_message.h"
#include "netlib_safequeue.h"
#include "netlib_session.h"
#include "netlib_module.h"

namespace netlib {

    template<typename T>
    class Server {
    public:

        Server(std::string localAddress, uint16_t port, T pingType, T pongType) {
            curId_ = port;
            context_ = new asio::io_context();
            pingType_ = pingType;
            pongType_ = pongType;
            localAddress_ = std::move(localAddress);
        }

        ~Server() = default;

        void start() {
            idleWork_ = new asio::io_context::work(*context_);
            contextThread_ = std::thread([this]() {context_->run();});
            std::cout << "[SERVER] Started" << "\n";
            prepareSession();
        }

        void stop() {
            context_->stop();
            if (contextThread_.joinable()) contextThread_.join();
            std::cout << "[SERVER] Stopped\n";
        }

        void prepareSession() {
            if (sessionsMap_.find(curId_) != sessionsMap_.end())
                return;
            std::shared_ptr<Session<T>> newSes =
                    std::make_shared<Session<T>>(context_, queueIn_, asio::ip::udp::socket(*context_, asio::ip::udp::v4()), curId_, pongType_);
            asio::ip::udp::endpoint localEndpoint = asio::ip::udp::endpoint(asio::ip::address_v4::from_string(localAddress_), curId_);
            newSes->bindToLocalEndpoint(localEndpoint);
            newSes->startStunSession();
            sessionsMap_[curId_] = newSes;
            modulesMap_[curId_] = {};
        }

        void addModule(uint16_t id, Module<T> *module) {
            if (modulesMap_.find(id) == modulesMap_.end() || sessionsMap_.find(id) == sessionsMap_.end())
                return;
            module->setContext(context_);
            module->addSession(sessionsMap_[id]);
            modulesMap_[id].push_back(module);
            modulesMap_[id].back()->start();
        }

        uint16_t connectToHost(std::string &host, uint16_t port) {
            try {
                asio::ip::udp::resolver resolver(*context_);
                asio::ip::udp::resolver::query query(host, std::to_string(port));
                asio::ip::udp::resolver::iterator iter = resolver.resolve(query);
                if (sessionsMap_.find(curId_) == sessionsMap_.end())
                    prepareSession();
                sessionsMap_[curId_]->connectWithEndpoint(*iter, pingType_);
                std::cout << "[SERVER]: New UDP connection to " << (*iter).endpoint() << " created on port " << curId_ << "\n";
                curId_++;
                prepareSession();
                return curId_ - 1;
            }
            catch (std::exception &ex) {
                std::cerr << ex.what() << "\n";
                return -1;
            }
        }

        void disconnectClient(uint16_t id) {
            if (sessionsMap_.find(id) == sessionsMap_.end())
                return;
            std::shared_ptr<Session<T>> client = sessionsMap_[id];
            sessionsMap_.erase(id);
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
                sessionsMap_.erase(id);
            }
        }

        void update() {
            while (!queueIn_.empty()) {
                OwnedMessage<T> msg = queueIn_.pop_front();
                uint16_t id = msg.session_->getId();
                bool shouldBeUpdated = true;
                for (Module<T> *mod: modulesMap_[id]) {
                    if (mod->checkReceive(msg.msg_)) {
                        if (mod->receive(msg.msg_)) {
                            shouldBeUpdated = false;
                            break;
                        }
                    }
                }
                if (shouldBeUpdated)
                    onMessage(msg);
            }
        }

        void pushEvent(Message<T> &msg) {
            eventQueue_.push_back(msg);
            if (!isEventUpdates_) {
                context_->post([this]() {
                   eventUpdate();
                });
            }
        }

        void eventUpdate() {
            if (eventQueue_.empty()) {
                isEventUpdates_ = false;
                return;
            }
            isEventUpdates_ = true;
            Message<T> event = eventQueue_.pop_front();
            uint16_t id;
            event >> id;
            if (modulesMap_.find(id) == modulesMap_.end())
                return;
            for (Module<T> *mod: modulesMap_[id]) {
                if (mod->checkSend(event)) {
                    if (mod->send(event))
                        break;
                }
            }
            context_->post([this]() {
                eventUpdate();
            });
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
        asio::io_context *context_;
        asio::io_context::work *idleWork_;
        std::thread contextThread_;

        SafeQueue<OwnedMessage<T>> queueIn_;
        std::map<uint16_t, std::shared_ptr<Session<T>>> sessionsMap_;

        std::map<uint16_t, std::vector<Module<T>*>> modulesMap_;

        uint16_t curId_ = 999;
        std::string localAddress_ = "0.0.0.0";

        bool isEventUpdates_ = false;
        SafeQueue<Message<T>> eventQueue_;

        T pingType_, pongType_;
    };
}