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
    std::vector<int> a = {1, 2, 3};
    msg << x << t << a;
    std::cout << msg << "\n";
    int y; float f;
    std::vector<int> b;
    msg >> b >> f >> y;
    std::cout << y << " " << f << "\n";
    for (int i: b)
        std::cout << i << " ";
    std::cout << "\n";
    std::cout << msg << "\n";
    return 0;
}
