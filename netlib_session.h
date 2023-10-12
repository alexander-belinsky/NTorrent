#pragma once
#include "netlib_header.h"
#include "netlib_message.h"
#include "netlib_safequeue.h"

namespace netlib {
    template<typename T>
    class Session : public std::enable_shared_from_this<Session<T>> {

    public:
        Session(asio::io_context &context, SafeQueue<Message<T>> &queueIn, asio::ip::tcp::socket &&socket, int id) :
        socket_(std::move(socket)), context_(context), queueIn_(queueIn)  {
            id_ = id;
            is_writing = false;
        }

        virtual ~Session() {}

        void send(const Message<T>& msg) {
            std::scoped_lock lock(mutex_);
            queueOut_.push_back(msg);
            asio::post(context_,
                   [this, msg] {
                    queueOut_.push_back(msg);
                    if (!is_writing) {
                        writeHeader();
                    }
                }
            );
        }

        void disconnect() {
            socket_.close();
        };

    private:

        void writeHeader() {
            is_writing = true;
            tempMsgOut_ = std::move(queueOut_.pop_back());
            asio::async_write(socket_, asio::buffer(&tempMsgOut_.header_, sizeof(MessageHeader<T>)),
                [this] (std::error_code er, size_t length) {
                    if (!er) {
                        if (tempMsgOut_.body_.size() > 0) {
                            writeBody();
                        }
                        else {
                            if (queueOut_.empty())
                                is_writing = false;
                            else
                                writeHeader();
                        }
                    }
                    else {
                        disconnect();
                    }
                }
            );
        }

        void writeBody() {
            asio::async_write(socket_, asio::buffer(tempMsgOut_.body_.data(), tempMsgOut_.body_.size()),
                [this] (std::error_code er, size_t length) {
                    if (!er) {
                        if (!queueOut_.empty())
                            writeHeader();
                        else
                            is_writing = false;
                    }
                    else {
                        disconnect();
                    }
                }
            );
        }

        void readHeader() {
            asio::async_read(socket_, asio::buffer(&tempMsgIn_.header_, sizeof(MessageHeader<T>)),
                 [this] (std::error_code er, size_t length) {

                }
            );
        }

    private:
        asio::ip::tcp::socket socket_;
        asio::io_context &context_;
        SafeQueue<Message<T>> queueOut_;
        Message<T> tempMsgOut_;
        SafeQueue<Message<T>> &queueIn_;
        Message<T> tempMsgIn_;
        std::mutex mutex_;
        bool is_writing;
        uint16_t id_;
    };
}
