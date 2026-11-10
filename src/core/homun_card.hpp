#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::homuncard {

// 3.0.11-beta.160 (task 9 of the remaining windows: the homunculus, Alt+R): the game's homunculus window (UI window
// 0x71, FUN_00467210 / FUN_00466be0) as a card in style A - its name and level, HP, SP and experience as bars, its
// fullness (0-100) and closeness (0-1000, the game's eight words), its eight stats (ATK MATK HIT CRIT DEF MDEF FLEE
// ASPD), the window's own buttons: feed (0xe0: the game asks first), its skills' window (0xdf), delete (0xe1: the
// game's red question first), ✕ (0x87). The parts that need no Windows; the words by GDI (win/homun_card).
// 3.0.11-beta.221: a skill of its in the card's list
struct SkillRow {
    std::string name;  // UTF-8
    int level = 0;
    bool up = false;  // can rise (the server's flag) - a «+» with points left
    int use = 0;      // 3.0.16-beta.31: the level it is used at (core/companion_skills useOf)
};
std::string skillName(int id);  // the homunculi's skills (8001-8016) as the server's skill_db names them; "" else
struct Info {
    std::string name;  // UTF-8
    int level = 0, hp = 0, maxHp = 0, sp = 0, maxSp = 0;
    long long exp = 0, maxExp = 0;
    int hunger = 0, intimacy = 0;
    int stats[7] = {0, 0, 0, 0, 0, 0, 0};  // ATK MATK HIT CRIT DEF MDEF FLEE
    int aspd = 0;                          // the game's delay (shown as (2000 - it) / 10)
    std::vector<SkillRow> skills;          // 3.0.11-beta.221
    int points = 0;                        // its skill points left
};
int intimacyState(int intimacy);  // 0 hostile ... 6 loyal, 7 unknown (the game's 3/10/100/250/750/910/1000)
int aspd(int delay);
enum class Button : uint8_t { Feed, Skills, Delete, Close };
int gameButton(Button b);

struct Texts {
    std::string title, level, hunger, intimacy;  // level: «Рівень {1}»
    std::string intimacyStates[8];
    std::string feed, skills, remove;
    std::string skillsHead, skillLevel;  // 3.0.11-beta.221: «Навички · вільних очок: {1}», «Рів. {1}»
};
struct Show {
    int hover = -1;  // a Button
    int focus = -1;  // the controller's (Feed or Skills)
    int hoverPlus = -1, hoverSkill = -1;  // 3.0.11-beta.221
    int hoverLess = -1, hoverMore = -1;   // 3.0.16-beta.31
    int padRow = -1;  // 3.0.16-beta.39: the controller's skill row (a gold edge)
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, name, level, hpLabel, hp, spLabel, sp, expLabel, exp, hungerLabel, hunger,
        intimacyLabel, intimacy, remove;
    panel::Box stats[8];
    panel::Box buttons[2];  // feed, skills (3.0.11-beta.221: w 0 - its skills listed below instead)
    panel::Box skillsHead;  // 3.0.11-beta.221
    std::vector<panel::Box> skillRows, skillPlus;
    std::vector<panel::Box> skillLess, skillMore, skillLevel;  // 3.0.16-beta.31: the use level's ‹ › and its words
};
Layout layout(int targetW, int targetH, float scale, int skillCount = 0);
enum class Part : uint8_t { None, Button, Plus, Skill, Less, More };  // 3.0.16-beta.31: Less, More
struct Hit {
    Part part = Part::None;
    Button button = Button::Close;
    int index = -1;  // 3.0.11-beta.221: a skill's row
};
Hit hitTest(const Layout& L, float x, float y);


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

}  // namespace uagfx::homuncard
