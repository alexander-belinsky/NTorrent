#include "netlib.h"
#include <iostream>

enum class MsgTypes {
    Connect,
    IntMessage,
    Fire,
};

class CustomServer: public netlib::Server<MsgTypes> {
public:

    CustomServer(uint16_t port) : netlib::Server<MsgTypes>(port)
    {
    }

    bool onConnect(std::shared_ptr<netlib::Session<MsgTypes>> session) {
        std::cout << "New connection: " << session->getEndpoint() << " with id " << session->getId() << "\n";
        return true;
    }

    void onMessage(netlib::OwnedMessage<MsgTypes> &msg) {
        std::cout << "New message from " << msg.session_->getId() << "\n";
        int msgContent;
        msg.msg_ >> msgContent;
        std::cout << "  Content: " << msgContent << "\n";
    }

    void onDisconnect(std::shared_ptr<netlib::Session<MsgTypes>> session) {
        std::cout << "Disconnection from: " << session->getId() << "\n";
    }
};

void updateCircle(CustomServer &Server) {
    while (1) {
        Server.update();
    }
};

int main() {
    uint16_t port;
    std::cin >> port;
    CustomServer Server(port);
    Server.start();
    std::thread thread(updateCircle, std::ref(Server));
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
            int msgContent;
            std::cin >> id >> msgContent;
            netlib::Message<MsgTypes> msg(MsgTypes::IntMessage);
            msg << msgContent;
            Server.sendMessage(id, msg);
        } else if (command == 'e') {
            break;
        }
    }
    return 0;
}
