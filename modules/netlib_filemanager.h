#pragma once
#include "../netlib_header.h"
#include "../netlib_session.h"
#include "../netlib_module.h"

namespace netlib {
    template<typename T>
    class FileManager : public Module<T> {
    public:
        const uint32_t CHUNK_SIZE = 16384;

        FileManager(T startType, T sendType, T finishType, T sendEventType, std::string &path) {
            isRunning_ = false;

            startType_ = startType;
            sendType_ = sendType;
            finishType_ = finishType;
            sendEventType_ = sendEventType;

            downloadPath_ = path;
        }

        bool receive(Message<T> &msg) {
            getFile(msg);
            return true;
        }

        bool checkReceive(Message<T> &msg) {
            return msg.header_.id_ == startType_ || msg.header_.id_ == sendType_ || msg.header_.id_ == finishType_;
        }

        bool checkSend(Message<T> &msg) {
            return msg.header_.id_ == sendEventType_;
        }

        bool send(Message<T> &msg) {
            std::string path, name;
            msg >> path >> name;
            sendFile(path, name);
            return true;
        }

        void setContext(asio::io_context *context) {
            context_ = context;
        }

        void addSession(const std::shared_ptr<Session<T>> session) {
            session_ = session;
        }

        void start() {};

        void sendFile(std::string &path, std::string &name) {
            std::cout << "sendFile func\n";
            queue.push_back(std::make_pair(path, name));
            if (!isRunning_) {
                context_->post([this]() {
                    startSending();
                });
            }
        }

        void startSending() {
            std::pair<std::string, std::string> tempPair = queue.pop_front();
            std::string curPath = tempPair.first;
            std::string name = tempPair.second;
            std::cout << "Start Sending " << curPath << " " << name;
            isRunning_ = true;
            Message<T> tempMsg;
            tempMsg.header_.id_ = startType_;
            try {
                fin.open(curPath, std::ios::binary);
                std::cout << fin.is_open() << "\n";
            } catch (std::exception &ex) {
                isRunning_ = false;
                std::cout << "[FileManager] Error during opening file\n";
                return;
            }
            tempMsg << name;
            session_->send(tempMsg);
            context_->post([this]() {
                sendChunk();
            });
        }

        void sendChunk() {
            if (!fin.is_open()) {
                finishSending();
                return;
            }

            std::vector<char> contVec;
            char temp_c;
            for (int i = 0; i < CHUNK_SIZE; i++) {
                if (!fin.read(&temp_c, 1))
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
            session_->send(tempMsg);

            context_->post([this]() {
                sendChunk();
            });
        }

        void finishSending() {
            fin.close();
            Message<T> tempMsg;
            tempMsg.header_.id_ = finishType_;
            session_->send(tempMsg);
            isRunning_ = false;
            if (!queue.empty())
                startSending();
        }

        void getFile(Message<T> &msg) {
            if (msg.header_.id_ == startType_) {
                std::string name;
                msg >> name;
                fout.open(downloadPath_ + "\\" + name, std::ios::binary);
                fout.clear();
                std::cout << "Got file. Name: " << name << " " << fout.is_open() << "\n";
            } else if (msg.header_.id_ == sendType_) {
                //std::cout << "sendType " << fout.is_open() << "\n";
                if (!fout.is_open())
                    return;
                std::vector<char> contVec;
                msg >> contVec;
                std::cout << contVec.size() << "\n";
                for (char el: contVec) {
                    fout.write(&el, 1);
                }
            } else if (msg.header_.id_ == finishType_) {
                std::cout << "finishFile\n";
                fout.close();
            }

        }

    private:
        asio::io_context *context_;

        SafeQueue<std::pair<std::string, std::string>> queue;
        std::shared_ptr<Session<T>> session_;
        std::string downloadPath_;

        T startType_;
        T sendType_;
        T finishType_;

        T sendEventType_;

        bool isRunning_;

        std::ifstream fin;
        std::ofstream fout;
    };
}