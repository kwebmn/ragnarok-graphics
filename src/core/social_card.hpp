#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::socialcard {

// 3.0.11-beta.163 (tasks 5 and 6 of the remaining windows: the friends and the party): the game's friends and party
// window (UI window 0x22, FUN_004c2250 - its two tabs) as a card in style A: its tabs, a header (the party's name, the
// friends' count), a row a person (a dot online or not, ♛ the party's leader, where they are), the chosen person's
// actions the window's own buttons (6 / 0xa6 whisper, 0xa9 delete or expel - the game asks first, 0xaa leave - with
// one's own row chosen, 0xa8 settings, 0x87 close). The parts that need no Windows; the words by GDI
// (win/social_card).
enum class Tab : uint8_t { Friends, Party };
constexpr int kTabMsg = 0x95;
int tabParam(Tab t);  // the window's 0x95 with 1 (the friends) or 2 (the party)
enum class Action : uint8_t { Whisper, Remove, Leave, Settings, Close };
int gameButton(Action a);
std::vector<Action> actions(Tab t);  // the buttons shown (Close is the ✕)

struct Entry {
    std::string name, map;  // UTF-8
    bool online = false, leader = false, self = false;
};
bool allowed(Tab t, Action a, const Entry* chosen);  // chosen: nullptr none

struct Texts {
    std::string tabs[2], header, offline, empty;
    std::string actions[4];  // the tab's buttons' words, in actions(tab)'s order
};
struct Show {
    Tab tab = Tab::Friends;
    int selected = -1, scroll = 0;
    int hoverRow = -1, hoverButton = -1, hoverTab = -1;
    bool hoverClose = false;
    int focus = -1;  // the controller's button
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, close, header, list;
    panel::Box tabs[2];
    std::vector<panel::Box> rows;     // shown (10 at most)
    std::vector<panel::Box> buttons;  // actions(tab)'s
};
Layout layout(int targetW, int targetH, float scale, Tab tab, size_t count);
int mostScroll(const Layout& L, size_t count);
enum class Part : uint8_t { None, Tab, Row, Button, Close };
struct Hit {
    Part part = Part::None;
    int index = -1;  // a tab's, a row's (of all, the scroll counted), a button's
};
Hit hitTest(const Layout& L, int scroll, size_t count, float x, float y);


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const std::vector<Entry>& entries, const Show& sh, const Texts& t);

// 3.0.18-beta.34 (uaUI's friends and party): the entry chosen in the shown list (none: nullptr)
const Entry* chosenEntry(const std::vector<Entry>& entries, const Show& sh);

}  // namespace uagfx::socialcard
