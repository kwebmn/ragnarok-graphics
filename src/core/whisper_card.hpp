#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::whispercard {

// 3.0.11-beta.212 (the tester: «віспер чат - в старому стилі»): the game's 1:1 whisper windows (class 0x699BC8, one a
// partner) as cards in style A at the screen's left, cascaded - the partner's name, the lines (the newest at the
// bottom), the game's own line to type into (a click gives it the keys, Enter sends as before), «Надіслати» (its own
// send) and ✕. The parts that need no Windows; the words by GDI (win/whisper_window).
struct Line {
    std::string text;  // UTF-8
    uint32_t rgb = 0;  // 0xRRGGBB; 0 the card's own
};
struct Card {
    std::string name, input;  // UTF-8
    std::vector<Line> lines;  // all, the newest last
    bool focus = false, caret = false;
};
struct Texts {
    std::string with, send;  // «Шепіт: {1}», «Надіслати»
};
enum class Part : uint8_t { None, Body, Input, Send, Close };
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, lines, input, send;
    int lineRows = 0;
};
// The `n`th (from 0) cascaded down and right at the screen's left, inside it
Layout layout(int n, int targetW, int targetH, float scale);
Part hitTest(const Layout& L, float x, float y);  // the picture's pixels


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Card& c, Part hover, const Texts& t);

}  // namespace uagfx::whispercard
