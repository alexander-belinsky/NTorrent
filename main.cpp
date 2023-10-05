#include "netlib.h"
#include <iostream>

enum class MsgTypes {
    Connect,
    IntMessage,
    Fire,
};

int main() {
    netlib::Message<MsgTypes> msg(MsgTypes::Connect);
    int x = 10;
    float t = 12.5;
    msg << x << t;
    std::cout << msg << "\n";
    int y; float f;
    msg >> f >> y;
    std::cout << y << " " << f << "\n";
    std::cout << msg << "\n";
    return 0;
}
