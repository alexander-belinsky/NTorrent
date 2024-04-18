#pragma once

#include "netlib_header.h"

namespace netlib {
    template<typename T>
    struct MessageHeader {
        T id{};
        uint32_t size = 0;
    };

    template<typename T>
    struct Message {
        MessageHeader<T> m_header;
        std::vector<uint8_t> m_body;

        Message() = default;

        explicit Message(T id) {
            m_header.id = id;
        }

        T getId() const {
            return m_header.id;
        }

        uint32_t getSize() const {
            return m_header.size;
        }

        void clear() {
            m_header.size = 0;
            m_body.clear();
        }

        bool empty() {
            return m_header.size == 0;
        };

        friend std::ostream& operator << (std::ostream& os, const Message<T>& msg) {
            os << "Id: " << int(msg.getId()) << " Size: " << msg.getSize();
            return os;
        }

        friend netlib::Message<T>& operator << (netlib::Message<T>& msg, const std::string& data) {
            std::vector<char> vec;
            for (char c: data)
                vec.push_back(c);
            msg << vec;
            return msg;
        }

        friend netlib::Message<T>& operator >> (netlib::Message<T>& msg, std::string& data) {
            std::vector<char> vec;
            msg >> vec;
            data.clear();
            for (char c: vec)
                data.push_back(c);
            return msg;
        }

        template<typename D>
        friend netlib::Message<T>& operator << (netlib::Message<T>& msg, const std::vector<D>& data) {
            uint32_t sz = data.size();
            for (const D& item: data)
                msg << item;
            msg << sz;
            return msg;
        }

        template<typename D>
        friend netlib::Message<T>& operator >> (netlib::Message<T>& msg, std::vector<D>& data) {
            uint32_t sz;
            msg >> sz;
            data.resize(sz);
            for (uint32_t i = 0; i < sz; i++)
                msg >> data[sz - i - 1];
            return msg;
        }

        template<typename D>
        friend netlib::Message<T>& operator << (netlib::Message<T>& msg, const D& data) {
            size_t prevSz = msg.m_body.size();
            msg.m_body.resize(prevSz + sizeof(D));
            std::memcpy(msg.m_body.data() + prevSz, &data, sizeof(D));
            msg.m_header.size = msg.m_body.size();
            return msg;
        }

        template<typename D>
        friend netlib::Message<T>& operator >> (netlib::Message<T>& msg, D& data) {
            size_t newSz = msg.m_body.size() - sizeof(D);
            std::memcpy(&data, msg.m_body.data() + newSz, sizeof(D));
            msg.m_body.resize(newSz);
            msg.m_header.size = msg.m_body.size();
            return msg;
        }
    };

    template<typename T>
    class Session;

    template <typename T>
    struct OwnedMessage {
        Message<T> msg_;
        std::shared_ptr<Session<T>> session_;

        OwnedMessage(std::shared_ptr<Session<T>> session, Message<T> msg) : msg_(msg), session_(session){

        }
    };
}