#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::guildcard {

// 3.0.11-beta.165 (task 6 of the remaining windows: the guild, Alt+G): the game's guild window (each of its tabs a
// UI window of its own, 0x3C-0x41) as a card in style A with two tabs: its information (the name, the master, the
// level, the members of the most, their average level, the experience to the next level, the tax points, the
// territory - the server's 0x01b6 as the game keeps it) and its members (a row each: online or not, the name, the
// title, the job and the level - its 0x0154). ✕ the game's close (6 / 0x87). The parts that need no Windows; the
// words by GDI (win/guild_card).
constexpr int kWindows = 6, kCloseButton = 0x87;
constexpr uint32_t kFirstAt = 0x170;  // the manager's place of the first of them (4 apart)
enum class Tab : uint8_t { Info, Members };
struct Info {
    std::string name, master, territory;  // UTF-8
    int level = 0, members = 0, most = 0, average = 0, tax = 0;
    long long exp = 0, nextExp = 0;
};
struct Member {
    std::string name, title, job;  // UTF-8
    int level = 0;
    bool online = false;
};
struct Texts {
    std::string title, tabs[2], facts[7], none, online;  // online: «{1} з {2} у грі»
};
struct Show {
    Tab tab = Tab::Info;
    int scroll = 0;
    int hoverTab = -1;
    bool hoverClose = false;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, head, list;
    panel::Box tabs[2];
    panel::Box facts[7];
    std::vector<panel::Box> rows;  // the members shown (12 at most)
};
Layout layout(int targetW, int targetH, float scale, Tab tab, size_t members);
int mostScroll(const Layout& L, size_t members);
enum class Part : uint8_t { None, Tab, Close, Body };
struct Hit {
    Part part = Part::None;
    int index = -1;
};
Hit hitTest(const Layout& L, float x, float y);

void paint(panel::Picture& out, const Layout& L, const std::vector<Member>& members, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Info& g, const std::vector<Member>& members, const Show& sh,
                         const Texts& t);

}  // namespace uagfx::guildcard
