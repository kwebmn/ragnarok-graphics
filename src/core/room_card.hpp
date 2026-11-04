#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::roomcard {

// 3.0.11-beta.208 (the tester's third list: «alt c - вже створений чат - в старому стилі»): the game's chat room
// window (UI window 0x1C) as a card in style A at the screen's left - open or with a password, its title and count,
// its lines (the newest at the bottom), its members (the owner ★), the game's own line to type into (a click gives it
// the keys, Enter sends as before), «Вийти»; the owner's «Налаштування» and, a member chosen, «Вигнати» and «Зробити
// власником»; «У друзі» for anyone chosen. The parts that need no Windows; the words by GDI (win/room_window).
struct Line {
    std::string text;  // UTF-8
    uint32_t rgb = 0;  // 0xRRGGBB; 0 the card's own
};
struct Member {
    std::string name;  // UTF-8
    bool owner = false;
};
struct Card {
    std::string title;  // UTF-8
    bool open = true;
    int count = 0, limit = 0;
    std::vector<Line> lines;  // all of them, the newest last
    std::vector<Member> members;
    std::string input;  // the game's line, UTF-8
    bool focus = false, caret = false;
    bool mine = false;  // the room is one's own
    int chosen = -1;    // a member chosen: its actions shown
};
struct Texts {
    std::string open, locked, members, setup, leave, kick, owner, friendAdd;
};
enum class Part : uint8_t { None, Member, Input, Setup, Leave, Kick, Owner, Friend, Close };
struct Hit {
    Part part = Part::None;
    int index = -1;  // a member's
};
struct Show {
    Hit hover;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, badge, title, count, close, lines, membersHead, input, setup, leave, kick, owner, friendAdd;
    std::vector<panel::Box> memberRows;
    int lineRows = 0;  // the lines that fit
};
uint32_t rgbOf(uint32_t bgr);  // the game's 0x00BBGGRR as 0xRRGGBB
std::vector<Line> lastLines(const std::vector<Line>& lines, int rows);
// At the screen's left, a sixth down (inside the screen); a tool's box empty (w 0) when it is not shown
Layout layout(const Card& c, int targetW, int targetH, float scale);
Hit hitTest(const Layout& L, float x, float y);  // the picture's pixels

void paint(panel::Picture& out, const Layout& L, const Card& c, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Card& c, const Show& sh, const Texts& t);

}  // namespace uagfx::roomcard
