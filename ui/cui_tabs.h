#pragma once

#include "cui_header.h"
#include "Objects/cui_object.h"

namespace cui {

    struct Option {
        Option (std::string name_, int id_): name(std::move(name_)), id(id_) {
        }
        std::string name;
        int id;
    };

    class Tab {
    public:

        ~Tab() {
            for (Object* obj: m_objects) {
                delete obj;
            }
        }

        virtual void update() {
            for (Object* obj: m_objects) {
                obj->draw();
            }
            goToXY(30, 2);
            std::string text = "[MENU]";
            printText(text);
            drawOptions();
        }

        void drawOptions() {
            int x = 10;
            int y = 5;
            for (int i = 0; i < m_options.size(); i++) {
                goToXY(x, y);
                if (m_curOption == i) {
                    printText(m_options[i].name);
                } else {
                    printText(m_options[i].name, FOREGROUND_INTENSITY);
                }
                y++;
            }
        }

        virtual int updateCh(char ch) {
            switch (ch) {
                case UP: {
                    if (m_curOption > 0) {
                        m_curOption--;
                        beep();
                    }
                    break;
                }
                case DOWN: {
                    if (m_curOption < m_options.size() - 1) {
                        m_curOption++;
                        beep();
                    }
                    break;
                }
                case ENTER: {
                    beep();
                    return m_options[m_curOption].id;
                }
            }
            return -1;
        }

        bool isCursorVisible = false;
        std::vector<Option> m_options = {Option("[Connect to friend]", 1),
                                         Option("[Download File]", 2),
                                         Option("[Downloads List]", 3),
                                         Option("[Upload File]", 4)};

        std::vector<Object*> m_objects;
        int m_curOption = 0;
    };

    class InviteCodeTab: public Tab {
    public:
        InviteCodeTab(netlib::Node &node): m_node(node) {

        }

        void update() {
            goToXY(30, 2);
            std::string text = "[INVITE CODE]";
            printText(text);

            goToXY(5, 5);
            text = "Your invite code is: " + m_node.getInviteCode();
            printText(text);
            goToXY(5, 6);
            text = "Send it to your friend and get their code";
            printText(text);
            goToXY(5, 8);
            printText("Your friend's code:");
            goToXY(25, 9);
            printText("------------------", COMMON_LVB_GRID_HORIZONTAL | FOREGROUND_INTENSITY);
        }

        std::vector<Option> m_options = {};

        bool isCursorVisible = true;


        netlib::Node &m_node;
    };
}
