#pragma once

#include "netlib_header.h"
//#include "netlib_session.h"

namespace netlib {
    template<typename T>
    struct MessageHeader {
        T id_{};
        uint32_t size_ = 0;
    };

    template<typename T>
    struct Message {
        MessageHeader<T> header_;
        std::vector<uint8_t> body_;

        explicit Message(T id) {
            header_.id_ = id;
        }

        T getId() const {
            return header_.id_;
        }

        size_t getSize() const {
            return header_.size_;
        }

        friend std::ostream& operator << (std::ostream& os, const Message<T>& msg) {
            os << "Id: " << int(msg.getId()) << " Size: " << msg.getSize();
            return os;
        }

        template<typename D>
        friend netlib::Message<T>& operator << (netlib::Message<T>& msg, const D& data) {
            size_t prevSz = msg.body_.size();
            msg.body_.resize(prevSz + sizeof(D));
            std::memcpy(msg.body_.data() + prevSz, &data, sizeof(D));
            msg.header_.size_ = msg.body_.size();
            return msg;
        }

        template<typename D>
        friend netlib::Message<T>& operator >> (netlib::Message<T>& msg, D& data) {
            size_t newSz = msg.body_.size() - sizeof(D);
            std::memcpy(&data, msg.body_.data() + newSz, sizeof(D));
            msg.body_.resize(newSz);
            msg.header_.size_ = msg.body_.size();
            return msg;
        }
    };

    template<typename T>
    class Session;

    template <typename T>
    struct OwnedMessage {
        Message<T> msg_;
        std::shared_ptr<Session<T>> session_;

    };
}
