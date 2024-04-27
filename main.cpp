#include "netlib.h"
#include <iostream>

using namespace netlib;

void importSettings(std::string &localAddress, uint16_t  &port, std::string &downloadsPath) {
    std::ifstream fin("./.settings");
    fin >> localAddress >> port >> downloadsPath;
}

void printHex(uint64_t x) {
    std::vector<char> dic = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    std::string res;
    while (x > 0) {
        res = dic[x % 16] + res;
        x /= 16;
    }
    std::cout << res;
}

uint64_t fromHex(std::string s) {
    uint64_t x;
    std::reverse(s.begin(), s.end());
    x = 0;
    uint64_t pow = 1;
    for (char c: s) {
        if (c >= '0' && c <= '9') {
            x += pow * (c - '0');
        } else {
            x += pow * (c - 'a' + 10);
        }
        pow *= 16;
    }
    return x;
}

void readHex(uint64_t &x) {
    std::string s;
    std::cin >> s;
    x = fromHex(s);
}

void welcomeScreen() {
    std::cout << "        _      __        __                             __             __    __      \n"
                 "       | | /| / / ___   / / ____ ___   __ _  ___       / /_ ___       / /_  / /  ___ \n"
                 "       | |/ |/ / / -_) / / / __// _ \\ /  ' \\/ -_)     / __// _ \\     / __/ / _ \\/ -_)\n"
                 "       |__/|__/  \\__/ /_/  \\__/ \\___//_/_/_/\\__/      \\__/ \\___/     \\__/ /_//_/\\__/ \n"
                 "                   ____                                _  __       __                \n"
                 "                  / __/ __ __   ___  ___   ____       / |/ / ___  / /_               \n"
                 "                 _\\ \\  / // /  / _ \\/ -_) / __/      /    / / -_)/ __/               \n"
                 "                /___/  \\_,_/  / .__/\\__/ /_/        /_/|_/  \\__/ \\__/                \n"
                 "                             /_/                                                     \n\n";
    std::cout << "This net can be used to share files.\n";
    std::cout << "If you need some help try command help.\n";
    std::cout << "If you dont know how to use this net try command info.\n";
}

void informationScreen() {
    std::cout << "[Information]\n";
    std::cout << "When you upload your file, special .info file in the \"./Data\" appears.\n";
    std::cout << "Send this file to your friend.\n";
    std::cout << "Then if you are in one net they will be able to download it\n";
}

void helpScreen() {
    std::cout << "[Help]\n";
    std::cout << "invite - get invite code (send it to your friend)\n";
    std::cout << "connect <invite code> - connect by invite code\n";
    std::cout << "upload <path to file> - upload file to the net\n";
    std::cout << "download <path to .info file> - download file from the net\n";
    std::cout << "Please, dont use paths with Russian characters\n";
}

int main() {

    setlocale(LC_ALL, "");
    welcomeScreen();
    std::string localAddress, downloadsPath;
    uint16_t port;
    importSettings(localAddress, port, downloadsPath);
    netlib::Node node(localAddress, port, downloadsPath);
    while (1) {
        std::string command;
        std::cin >> command;
        if (command == "help") {
            helpScreen();
        } else if (command == "info") {
            informationScreen();
        } else if (command == "connect") {
            uint64_t inviteCode;
            readHex(inviteCode);
            node.addDirectConnection(inviteCode);
        } else if (command == "exit") {
            break;
        } else if (command == "invite") {
            printHex(node.getInviteCode());
            std::cout << "\n";
        } else if (command == "download") {
            std::string infoPath;
            std::cin >> infoPath;
            node.downloadFile(infoPath);
        } else if (command == "upload") {
            std::string filePath;
            std::cin >> filePath;
            node.uploadFile(filePath);
        } else {
            std::cout << "[Error]: There is no such command\n";
        }
    }
    node.stop();
    return 0;
}
