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

// 3.0.11-beta.80 (the owner: «чтобы видить какие касты стоят у персонажей какимито маленькими иконками»): a buff on a
// member (the server tells its icons to those near it - uAthena clif_status_change AREA), its time when a cast told it
struct Buff {
    const panel::Picture* icon = nullptr;  // premultiplied, about its box's side
    float left = -1;                       // its time left, a share (0..1); < 0 not known
    float glow = 0;  // 3.0.11-beta.81: one's own about to end - a gold edge round it this strong (0..1), pulsing
};

struct Member {
    std::string name;    // UTF-8
    std::string where;   // its map's words when not the character's map; "" here
    bool leader = false, online = true;
    int percent = -1;    // HP; < 0 not known
    const panel::Picture* face = nullptr;  // 3.0.11-beta.80: its head (premultiplied) in its round; null: its letter
    std::vector<Buff> buffs;               // 3.0.11-beta.80: its buffs as last told
    // 3.0.11-beta.81 (the owner: «1-8 делай»): its class and level after its name ("Priest 85"; "" not known), its
    // cast (the skill's words in their place, its part done as a line under its bar; < 0 none), dead
    std::string job, casting;
    float castPart = -1;
    bool dead = false;
    // 3.0.11-beta.82 (the owner's 2 and 4): here - its cells away (< 0 not known) and its way on the screen (a unit
    // vector, x right, y down); elsewhere its map's name with a route mark (a click goes there)
    int cells = -1;
    float dirX = 0, dirY = 0;
};
bool low(const Member& m, int threshold);  // here, online, known and under the threshold

struct Row {
    panel::Box box, round, name, value, bar;
    std::vector<panel::Box> buffs;  // 3.0.11-beta.80: its buffs' icons under its bar (as many as the row holds)
    panel::Box where;               // 3.0.11-beta.82: its arrow and cells away, at the end of its bar's line
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box glass;
    std::vector<Row> rows;
    // 3.0.11-beta.79 (the owner: «плашка с пати её надо иметь возможность сворачивать»): its header - «Група», the
    // members online of all, the fold (▾ / ▸); folded: the header alone
    panel::Box header, title, count, fold;
    bool folded = false;
    bool targeting = false;  // 3.0.11-beta.82: a skill waits for a target - the rows here edged gold
    // 3.0.11-beta.84 (the owner's 8): the view's button in the header (▦ compact, ▤ the list); compact - two columns
    // of small cells: a face, a name, an HP bar (its rows' boxes the cells)
    panel::Box view;
    bool compact = false;
};
Layout layout(size_t members, int left, int top, float scale, bool folded = false);
// 3.0.11-beta.80: each row's buffs counted - a row with any a line taller
std::vector<int> rowsOf(const std::vector<Member>& members);
Layout layout(const std::vector<int>& buffs, int left, int top, float scale, bool folded = false,
              bool compact = false);
enum class Part : uint8_t { None, Fold, Header, Row, View };
struct Hit {
    Part part = Part::None;
    int row = -1;
};
Hit hitTest(const Layout& L, float x, float y);  // the picture's pixels

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
                         const std::string& offline, const std::string& title = "", const std::string& dead = "");

}  // namespace uagfx::partypanel
