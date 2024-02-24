#pragma once
#include "../netlib_header.h"
#include "../netlib_module.h"

namespace netlib {

    template <typename T>
    class PingPongManager: public Module<T> {

        PingPongManager() {
            rng_ = std::mt19937(std::chrono::steady_clock::now().time_since_epoch().count());
        }

        uint64_t getRand() {
            return rng_();
        }

    public:

        bool checkReceive(Message<T> &msg) {
            return true;
        }

        bool receive(Message<T> &msg) {

            return false;
        }

        bool checkSend(Message<T> &msg) {
            return true;
        }

        bool send(Message<T> &msg) {

            return false;
        }

        void setContext(asio::io_context *context) {
            context_ = context;
        };

        void addSession(const std::shared_ptr<Session<T>> session) {
            session_ = session;
        };

    private:
        std::mt19937 rng_;

        asio::io_context *context_;
        std::shared_ptr<Session<T>> session_;

        //static std::map<uint64_t,
    };
}