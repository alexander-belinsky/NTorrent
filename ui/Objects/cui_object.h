#pragma once

#include "../cui_header.h"

namespace cui {
    class Object {
    public:
        int x, y;
        Object(int x_, int y_): x(x_), y(y_){

        }

        virtual void draw() {

        }

        virtual int maxY() {
            return y + 3;
        }
    };
}
