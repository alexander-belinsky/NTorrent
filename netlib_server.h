#pragma once
#include "netlib_header.h"
#include "netlib_session.h"
#include "netlib_message.h"
#include "netlib_safequeue.h"
#include "netlib_session.h"

namespace netlib {

    template<typename T>
    class Server {




    protected:

        virtual bool onConnect(Session<T> &session);

        virtual void onMessage();

    protected:
        asio::io_context context_;
        asio::ip::tcp::acceptor acceptor_;
        std::thread contextThread_;
        SafeQueue<OwnedMessage<T>> queueIn_;
        uint16_t curId;
    };
}