#include "netlib.h"
#include <iostream>

#define CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

enum class MsgTypes : uint16_t {
    StringMessage,
    PingMessage,
    PongMessage,

    StartFile,
    SendFile,
    FinishFile,

    // Events:

    SendFileEvent,

};

std::ostream& operator << (std::ostream &out, MsgTypes el) {
    out << (uint16_t) el;
    return out;
}

inline void EnableMemLeakCheck()
{
    _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) | _CRTDBG_LEAK_CHECK_DF);
}

class CustomServer: public netlib::Server<MsgTypes> {
public:

    CustomServer(std::string l, uint16_t port, MsgTypes pingType, MsgTypes pongType) : netlib::Server<MsgTypes>(l, port, pingType, pongType)
    {
    }

    void onMessage(netlib::OwnedMessage<MsgTypes> &msg) {
        //std::cout << "New message from " << msg.session_->getId() << "\n";
        if (msg.msg_.header_.id_ == MsgTypes::PingMessage) {
            if (!isConnected_) {
                std::cout << "Ping\n";
                isConnected_ = true;
            }
        } else if (msg.msg_.header_.id_ == MsgTypes::StringMessage) {
            std::string msgContent;
            msg.msg_ >> msgContent;
            std::cout << "  Content: " << msgContent << "\n";
        }
    }

    void onDisconnect(std::shared_ptr<netlib::Session<MsgTypes>> session) {
        std::cout << "Disconnection from: " << session->getId() << "\n";
    }

    bool isConnected_ = false;
};

void updateCircle(CustomServer &Server, bool &running) {
    while (running) {
        Server.update();
    }
};

int main() {
    EnableMemLeakCheck();
    std::string localAddress;
    uint16_t port;
    std::cin >> localAddress >> port;

    CustomServer Server(localAddress, port, MsgTypes::PingMessage, MsgTypes::PongMessage);
    Server.start();
    bool flag = true;
    std::thread thread(updateCircle, std::ref(Server), std::ref(flag));
    std::string downloadPath;
    std:: cin >> downloadPath;
    while (1) {
        char command;
        std::cin >> command;
        if (command == 'c') {
            std::string host;
            uint16_t conPort;
            std::cin >> host >> conPort;
            uint16_t id = Server.connectToHost(host, conPort);
            auto *fileManager = new netlib::FileManager(MsgTypes::StartFile, MsgTypes::SendFile, MsgTypes::FinishFile, MsgTypes::SendFileEvent, downloadPath);
            Server.addModule(id, fileManager);
        } else if (command == 'm') {
            uint16_t id;
            std::string msgContent;
            std::cin >> id >> msgContent;
            netlib::Message<MsgTypes> msg(MsgTypes::StringMessage);
            msg << msgContent;
            Server.sendMessage(id, msg);
        } else if (command == 'e') {
            break;
        } else if (command == 'g') {
            std::cout << Server.getRealEp() << "\n";
        } else if (command == 'a') {
            uint16_t id;
            std::cin >> id;
        } else if (command == 'f') {
            uint16_t id;
            std::string path;
            std::string name;
            std::cin >> id;
            std::cin >> path >> name;
            netlib::Message<MsgTypes> msg(MsgTypes::SendFileEvent);
            msg << name << path << id;
            Server.pushEvent(msg);
        }
    }
    flag = false;
    if (thread.joinable())
        thread.join();
    Server.stop();
    return 0;
}
