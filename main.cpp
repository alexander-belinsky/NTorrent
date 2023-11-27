#include "netlib.h"
#include <iostream>

enum class MsgTypes : uint16_t {
    StringMessage,
};

class CustomServer: public netlib::Server<MsgTypes> {
public:

    CustomServer(uint16_t port) : netlib::Server<MsgTypes>(port)
    {
    }

    void onMessage(netlib::OwnedMessage<MsgTypes> &msg) {
        std::cout << "New message from " << msg.session_->getId() << "\n";
        std::string msgContent;
        msg.msg_ >> msgContent;
        std::cout << "  Content: " << msgContent << "\n";
    }

    void onDisconnect(std::shared_ptr<netlib::Session<MsgTypes>> session) {
        std::cout << "Disconnection from: " << session->getId() << "\n";
    }
};

void updateCircle(CustomServer &Server, bool &running) {
    while (running) {
        Server.update();
    }
};

int main() {
    uint16_t port;
    std::cin >> port;
    CustomServer Server(port);
    Server.start();
    bool flag = true;
    std::thread thread(updateCircle, std::ref(Server), std::ref(flag));
    while(1) {
        char command;
        std::cin >> command;
        if (command == 'c') {
            std::string host;
            uint16_t conPort;
            std::cin >> host >> conPort;
            Server.connectToHost(host, conPort);
        } else if (command == 'm') {
            uint16_t id;
            std::string msgContent;
            std::cin >> id >> msgContent;
            netlib::Message<MsgTypes> msg(MsgTypes::StringMessage);
            msg << msgContent;
            Server.sendMessage(id, msg);
        } else if (command == 'e') {
            break;
        }
    }
    flag = false;
    thread.join();
    Server.stop();
    return 0;
}
