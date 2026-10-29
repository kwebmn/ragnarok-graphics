#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::partypanel {

// 3.0.11-beta.39 (stage 7 of the new interface, the HUD; the owner: «делай»): the party at the top left under the
// character block, style A (the concept's v19_cave) - a row a member: its name (a crown on the leader), its HP per
// cent at the right with a 5 px bar under the name; low HP red with «!», the row edged red; on another map its
// map's name instead, offline its word, no bar. Its SP never: the server sends none (clif_party_hp). The parts that
// need no Windows; the words go over it by GDI (win/party_panel).

struct Member {
    std::string name;    // UTF-8
    std::string where;   // its map's words when not the character's map; "" here
    bool leader = false, online = true;
    int percent = -1;    // HP; < 0 not known
};
bool low(const Member& m, int threshold);  // here, online, known and under the threshold

struct Row {
    panel::Box box, round, name, value, bar;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box glass;
    std::vector<Row> rows;
};
Layout layout(size_t members, int left, int top, float scale);

void paint(panel::Picture& out, const Layout& L, const std::vector<Member>& members, int threshold);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const std::vector<Member>& members, int threshold,
                         const std::string& offline);

}  // namespace uagfx::partypanel
