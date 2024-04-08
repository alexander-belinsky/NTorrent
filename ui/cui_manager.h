#pragma once

#include <utility>

#include "cui_header.h"
#include "Objects/cui_object.h"
#include "cui_tabs.h"

namespace cui {

    class ConsoleUI {
    public:
        explicit ConsoleUI(netlib::Node &node): m_node(node)  {
            consoleCursorVisible(false, 100);
        }

        void update() {
            system("CLS");
            consoleCursorVisible(false, 100);
            m_tabs[curTabId]->update();
            goToXY(1, 1);
            printText(std::to_string(curTabId));
            char ch = _getch();
            int x = m_tabs[curTabId]->updateCh(ch);
            if (x != -1)
                curTabId = x;
        }

        void addTab(Tab* tab) {
            m_tabs.push_back(tab);
        }

    private:
        std::vector<Tab*> m_tabs;
        int curTabId = 0;
        netlib::Node &m_node;
    };
}