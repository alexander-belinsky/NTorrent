#pragma once
#include "netlib_header.h"
#include "netlib_message.h"
#include "netlib_safequeue.h"

namespace netlib {
    template<typename T>
    class Session : public std::enable_shared_from_this<Session<T>> {

    public:
        Session(asio::io_context &context, SafeQueue<OwnedMessage<T>> &queueIn, asio::ip::tcp::socket &&socket, int id) :
        socket_(std::move(socket)), context_(context), queueIn_(queueIn)  {
            id_ = id;
            is_writing = false;
        }

        virtual ~Session() = default;

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

        void asyncDisconnect() {
            if (isConnected())
                asio::post(context_, [this]{socket_.close();});
        }

        bool isConnected() const {
            return socket_.is_open();
        }

        void connect(asio::ip::tcp::resolver::results_type &ep) {
            asio::async_connect(socket_, ep,
                [this] (std::error_code ec, asio::ip::tcp::endpoint &ep) {
                    if (!ec) {
                        readHeader();
                    }
                }
            );
        }

        void startListening() {
            asio::post(context_,
                [this] () {
                    readHeader();
                }
            );
        }

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
                    if (!er) {
                        if (tempMsgIn_.header_.size_ > 0) {
                            tempMsgIn_.body_.resize(tempMsgIn_.header_.size_);
                            readBody();
                        }
                        else {
                            addToQueue();
                        }
                    }
                    else {
                        disconnect();
                    }
                }
            );
        }

        void readBody() {
            asio::async_read(socket_, asio::buffer(&tempMsgIn_.body_.data(), tempMsgIn_.body_.size()),
                 [this] (std::error_code er, size_t length) {
                     if (!er) {
                         addToQueue();
                     }
                     else {
                         disconnect();
                     }
                 }
            );
        }

        void addToQueue() {
            queueIn_.push_back({this->shared_from_this(), tempMsgIn_});
            tempMsgIn_.clear();
            readHeader();
        }

    private:
        asio::ip::tcp::socket socket_;
        asio::io_context &context_;
        SafeQueue<Message<T>> queueOut_;
        Message<T> tempMsgOut_;
        SafeQueue<OwnedMessage<T>> &queueIn_;
        Message<T> tempMsgIn_;
        std::mutex mutex_;
        bool is_writing;
        uint16_t id_;
    };
}
