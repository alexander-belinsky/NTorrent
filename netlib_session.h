#pragma once
#include "netlib_header.h"
#include "netlib_message.h"
#include "netlib_safequeue.h"
namespace netlib {
    template<typename T>
    class Session : public std::enable_shared_from_this<Session<T>> {
    public:
        Session(asio::ip::tcp::socket &&socket, int id) : socket_(std::move(socket)) {
            id_ = id;
        }

        virtual ~Session() {}

    private:
        asio::ip::tcp::socket socket_;
        asio::io_context &context_;
        SafeQueue<Message<T>> queueOut_;
        SafeQueue<Message<T>> &queueIn_;

        
    };
}
