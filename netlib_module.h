#pragma once

namespace netlib {
    template<typename T>
    class Module {
    public:

        virtual bool receive(Message<T> &msg) {
            return false;
        };

        virtual bool checkReceive(Message<T> &msg) {
            return false;
        };

        virtual bool send(Message<T> &msg) {
            return false;
        };

        virtual bool checkSend(Message<T> &msg) {
            return false;
        };

        virtual void setContext(asio::io_context *context) {};

        virtual void addSession(const std::shared_ptr<Session<T>> session) {};

        virtual void start() {};
    };
}
