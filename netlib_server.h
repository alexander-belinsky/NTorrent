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

        Server(uint16_t port) :
        acceptor_(context_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port)) {

        }

        ~Server() {
            stop();
        }

        void start() {
            waitForConnection();
            contextThread_ = std::thread([this]() {context_.run();});
            std::cout << "[SERVER] Started: " << acceptor_.local_endpoint() << "\n";
        }

        void stop() {
            context_.stop();
            if (contextThread_.joinable()) contextThread_.join();
            std::cout << "[SERVER] Stopped\n";
        }

        void connectToHost(std::string &host, uint16_t port) {
            try {
                asio::ip::tcp::resolver resolver(context_);
                asio::ip::tcp::resolver::query query(host, std::to_string(port));
                asio::ip::tcp::resolver::results_type endpoints = resolver.resolve(query);
                std::cout << endpoints->endpoint().address() << "\n";
                std::shared_ptr<Session<T>> newSes =
                        std::make_shared<Session<T>>(context_, queueIn_, asio::ip::tcp::socket(context_), curId_);
                sessionsMap_[curId_] = std::move(newSes);
                sessionsMap_[curId_]->connectWithEndpoint(endpoints);
                curId_++;
            }
            catch (std::exception &ex) {
                std::cerr << ex.what() << "\n";
            }
        }

        void sendMessage(uint16_t id, Message<T> &msg) {
            if (sessionsMap_.find(id) == sessionsMap_.end())
                return;
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

    private:
        void waitForConnection() {
            acceptor_.async_accept(
            [this] (std::error_code er, asio::ip::tcp::socket sock) {
                if (!er) {
                    std::cout << "[SERVER] New connection\n";
                    std::shared_ptr<Session<T>> newSes =
                            std::make_shared<Session<T>>(context_, queueIn_, std::move(sock), curId_);
                    if (onConnect(newSes)) {
                        sessionsMap_[curId_] = std::move(newSes);
                        sessionsMap_[curId_]->startListening();
                        curId_++;
                    }
                } else {
                    std::cerr << "[SERVER] Connection error\n";
                }
                waitForConnection();
            }
            );
        }




    protected:

        virtual bool onConnect(std::shared_ptr<Session<T>> session) {
            return true;
        };

        virtual void onMessage(OwnedMessage<T> &msg) {

        };

        virtual void onDisconnect(std::shared_ptr<Session<T>> session) {

        };

    protected:
        asio::io_context context_;
        asio::ip::tcp::acceptor acceptor_;
        std::thread contextThread_;
        SafeQueue<OwnedMessage<T>> queueIn_;
        std::map<uint16_t, std::shared_ptr<Session<T>>> sessionsMap_;
        uint16_t curId_ = 999;
    };
}