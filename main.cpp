#include "netlib.h"
#include "ui/cui.h"
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

int main() {

    setlocale(LC_ALL, "");

    std::string localAddress, downloadsPath;
    uint16_t port;
    importSettings(localAddress, port, downloadsPath);
    netlib::Node node(localAddress, port, downloadsPath);
    while (1) {
        std::string command;
        std::cin >> command;
        if (command == "connect") {
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
        }
    }
    node.stop();
    return 0;
}
