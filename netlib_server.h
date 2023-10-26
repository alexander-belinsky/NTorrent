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

        }

        void stop() {
            context_.stop();
            if (contextThread_.joinable()) contextThread_.join();
        }

    private:
        void waitForConnection() {
            acceptor_.async_accept(
            [this] (std::error_code er, asio::ip::tcp::socket sock) {
                if (!er) {
                    std::cout << "[SERVER] New connection\n";
                    std::shared_ptr<Session<T>> new_ses = std::make_shared<Session<T>>(context_, queueIn_, std::move(sock), curId);
                    if (onConnect(new_ses)) {
                        sessionsMap_[curId] = new_ses;
                    }
                } else {
                    std::cerr << "[SERVER] Connection error\n";
                }
                waitForConnection();
            }
            );
        }


    protected:

        virtual bool onConnect(std::shared_ptr<Session<T>> session);

        virtual void onMessage(OwnedMessage<T> &msg);

        virtual void onDisconnect(std::shared_ptr<Session<T>> session);

    protected:
        asio::io_context context_;
        asio::ip::tcp::acceptor acceptor_;
        std::thread contextThread_;
        SafeQueue<OwnedMessage<T>> queueIn_;
        std::map<uint16_t, std::shared_ptr<Session<T>>> sessionsMap_;
        uint16_t curId = 999;
    };
}