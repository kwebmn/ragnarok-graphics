#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::loginchrome {

// 3.0.11-beta.173 (the login concept): the login screen's strip at the screen's foot, shared by its views - the
// server's status pill in the middle (core/server_status says its words). The parts that need no Windows; the words
// by GDI (win/login_chrome).
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;          // the strip in the target's pixels
    panel::Box status, statusDot, statusText;  // in the strip; 0 wide when there are no words
};
Layout layout(int targetW, int targetH, float scale, const std::string& statusText);
struct Show {
    std::string status;  // UTF-8
    uint32_t statusRgb = 0x9AA3B5;
};
void paint(panel::Picture& out, const Layout& L, const Show& sh);
enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xC9CFDB;
    float px = 14;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Show& sh);

}  // namespace uagfx::loginchrome
