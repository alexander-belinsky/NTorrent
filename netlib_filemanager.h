#pragma once
#include "netlib_header.h"
#include "netlib_session.h"

namespace netlib {
    template<typename T>
    class FileManager {

        const uint8_t CHUNK_SIZE = 128;

        FileManager(Session<T> ses, asio::io_context &context, T startType, T sendType, T finishType): session_(ses), context_(context) {
            isRunning = false;
            startType_ = startType;
            sendType_ = sendType;
            finishType_ = finishType;
        }

        bool sendFile(std::string &path, std::string &name) {
            queue.push_back(std::make_pair(path, name));
            if (!isRunning) {
                startSending(name);
            }
        }

        void startSending() {
            std::pair<std::string, std::string> tempPair = queue.pop_back();
            std::string curPath = tempPair.first;
            std::string &name = tempPair.second;
            isRunning = true;
            Message<T> tempMsg;
            tempMsg.header_.id_ = startType_;
            try {
                fin.open(curPath);
            } catch (std::exception &ex) {
                isRunning = false;
                std::cout << "[FileManager] Error during opening file\n";
                return;
            }
            tempMsg << name;
            session_.send(tempMsg);
            asio::post(context_, [this]() {
                sendChank();
            });
        }

        void sendChank() {
            if (!fin.is_open()) {
                finishSending();
                return;
            }

            std::vector<char> contVec;
            char temp_c;
            for (int i = 0; i < CHUNK_SIZE; i++) {
                if (!(fin >> temp_c))
                    break;
                contVec.push_back(temp_c);
            }
            if (contVec.empty()) {
                finishSending();
                return;
            }

            Message<T> tempMsg;
            tempMsg.header_.id_ = sendType_;
            tempMsg << contVec;
            session_.send(tempMsg);

            asio::post(context_, [this]() {
                sendChank();
            });
        }

        void finishSending() {
            fin.close();
            Message<T> tempMsg;
            tempMsg.header_.id_ = finishType_;
            session_.send(tempMsg);
            isRunning = false;
            if (!queue.empty())
                startSending();
        }

    private:
        asio::io_context &context_;
        SafeQueue<std::pair<std::string, std::string>> queue;
        Session<T> &session_;
        T startType_;
        T sendType_;
        T finishType_;
        bool isRunning;
        std::ifstream fin;
        std::ofstream fout;
    };
}