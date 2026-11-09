#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::merccard {

// 3.0.11-beta.161 (task 9 of the remaining windows: the mercenary, Ctrl+R): the game's mercenary window (UI window
// 0x84, FUN_00468100 / FUN_00467be0) as a card in style A - its kind (the game's title by its class), its name and
// level, HP and SP bars, the contract's end, loyalty, summons and kills, its eight stats (ATK MATK HIT CRIT DEF MDEF
// FLEE ASPD), the window's own buttons: its skills' window (0x11b), dismiss (0x11c: the game asks first), ✕ (0x87).
// The parts that need no Windows; the words by GDI (win/merc_card).
enum class Kind : uint8_t { Archer, Spearman, Swordsman };
Kind kindOf(int cls);  // 6017-6026 archers, 6037-6046 spearmen, else swordsmen
// 3.0.11-beta.218: a skill of its in the card's list (the game's own skills window lists nothing for them)
struct SkillRow {
    std::string name;  // UTF-8
    int level = 0;
    int use = 0;          // 3.0.16-beta.31: the level it is used at (core/companion_skills useOf)
    bool usable = false;  // 3.0.16-beta.31: in the game's own list (0x771444) - used, dragged onto the hotbar
};
struct Info {
    Kind kind = Kind::Swordsman;
    std::string name, ends;  // UTF-8; ends: the contract's end as the window part writes it
    int level = 0, hp = 0, maxHp = 0, sp = 0, maxSp = 0;
    int loyalty = 0, summons = 0, kills = 0;
    int stats[7] = {0, 0, 0, 0, 0, 0, 0};  // ATK MATK HIT CRIT DEF MDEF FLEE
    int aspd = 0;                          // the game's delay (shown as (2000 - it) / 10)
    std::vector<SkillRow> skills;          // 3.0.11-beta.218
};
enum class Button : uint8_t { Skills, Dismiss, Close };
int gameButton(Button b);

struct Texts {
    std::string kinds[3], level;  // level: «Рівень {1}»
    std::string facts[4];         // «Контракт до», «Вірність», «Викликів», «Убито»
    std::string skills, dismiss;
    std::string skillLevel;  // 3.0.11-beta.218: «Рів. {1}»
};
struct Show {
    int hover = -1;  // a Button
    bool focus = false;  // the controller on «Навички»
    int hoverSkill = -1, hoverLess = -1, hoverMore = -1;  // 3.0.16-beta.31
    int padRow = -1;  // 3.0.16-beta.39: the controller's skill row (a gold edge)
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, name, level, hpLabel, hp, spLabel, sp, skills, dismiss;
    panel::Box facts[4];
    panel::Box stats[8];
    std::vector<panel::Box> skillRows;  // 3.0.11-beta.218: under «Навички» (its heading `skills`), one at least
    std::vector<panel::Box> skillLess, skillMore, skillLevel;  // 3.0.16-beta.31: the use level's ‹ › and its words
};
Layout layout(int targetW, int targetH, float scale, int skillCount = 0);
enum class Part : uint8_t { None, Button, Skill, Less, More };  // 3.0.16-beta.31: a skill's row, ‹ ›
struct Hit {
    Part part = Part::None;
    Button button = Button::Close;
    int index = -1;  // 3.0.16-beta.31: the skill's row
};
Hit hitTest(const Layout& L, float x, float y);

void paint(panel::Picture& out, const Layout& L, const Info& i, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Info& i, const Show& sh, const Texts& t);

}  // namespace uagfx::merccard
