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
enum class Tab : uint8_t { Info, Members, Positions, Skills, Expelled, Notice };  // .220: all six the card's
// 3.0.11-beta.210 (the tester: «alt g відкриває режим гільди, але у ГМа немає купи функцій»): all six of the game's
// tabs on the card (two rows of three) - a click switches the game's own (its message 0x45 with the tab; the game
// checks its rights, the 0x0168 bits); the information and the members drawn by the card, the positions, the guild's
// skills, the expelled and the notice the game's own windows for now (all of a master's tools in them)
constexpr int kTabs = 6, kTabMsg = 0x45;
// 3.0.11-beta.220: the game's own window shows only while «Змінити» edits the positions or the notice (`editing`)
bool gameDrawn(int tab, bool editing);
bool allowed(int tab, uint32_t rights);  // the game's rights for the tab (members 1, positions 2, expelled 0x10, ...)
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
// 3.0.11-beta.220: tabs 2-5's rows (a position, a skill, one expelled: three columns, a «+» on a skill that can rise)
// and the notice
struct Line {
    std::string left, middle, right;  // UTF-8
    bool plus = false;
};
struct More {
    std::vector<Line> lines;
    std::string head;           // over them: the positions' count, the skill points left, ...
    std::string subject, body;  // the notice
};
struct Texts {
    std::string title, tabs[kTabs], facts[7], none, online;  // online: «{1} з {2} у грі»
    std::string edit;                                        // 3.0.11-beta.220: «Змінити»
    // 3.0.11-beta.271 (the tester: «вікно формування привітання в гільдії - в старому стилі»): the notice edited on
    // the card - «Тема», «Текст», «Зберегти», «Скасувати», the keys' hint
    std::string fieldTitle, fieldBody, save, cancel, editHint;
};
// .271: the server's notice: its subject up to 59 bytes, its body up to 119 (the game's code page)
constexpr size_t kSubjectMost = 59, kBodyMost = 119;
struct Show {
    Tab tab = Tab::Info;
    uint32_t rights = 0xFF;  // 3.0.11-beta.210: the game's tab rights (a tab not allowed dimmed)
    int scroll = 0;
    int hoverTab = -1;
    bool hoverClose = false;
    int hoverPlus = -1;      // 3.0.11-beta.220
    bool hoverEdit = false;  // 3.0.11-beta.220
    bool canEdit = false;    // 3.0.11-beta.220: «Змінити» shown (the tab's rights)
    // 3.0.11-beta.271: the notice being edited on the card - its fields (UTF-8), the one typed into (0 the subject),
    // the caret shown
    bool editing = false;
    std::string subject, body;
    int field = 0;
    bool caret = true;
    int hoverField = -1;
    bool hoverCancel = false;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, head, list;
    panel::Box tabs[kTabs];
    panel::Box facts[7];
    std::vector<panel::Box> rows;  // the members shown (12 at most); 3.0.11-beta.220: the positions, skills, expelled
    std::vector<panel::Box> plus;  // 3.0.11-beta.220: the skills' «+», one a row
    panel::Box notice, edit;       // 3.0.11-beta.220: the notice's text; «Змінити» (w 0: none)
    panel::Box fields[2], cancel;  // 3.0.11-beta.271: editing - the subject's and the body's fields; «Скасувати»
    // 3.0.19-beta.1 (the testers: «не видно значок ГІ»): «Інформація»'s head - the guild's emblem (w 0: another tab),
    // its level and members under the name (the name in `head`)
    panel::Box emblem, sub;
    // 3.0.19-beta.1: its own width at its size - the live card's width (uikit guild's refit leaves it: w from the
    // document as it is handed the old width back when the corner pulled it bigger)
    int wide = 0;
};
// .271: `editing` - the notice's fields, «Зберегти» in «Змінити»'s place and «Скасувати» beside it
Layout layout(int targetW, int targetH, float scale, Tab tab, size_t members, bool editing = false);
int mostScroll(const Layout& L, size_t members);
enum class Part : uint8_t { None, Tab, Close, Body, Plus, Edit, Field, Cancel };  // .271: Field, Cancel
struct Hit {
    Part part = Part::None;
    int index = -1;
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
std::vector<Words> words(const Layout& L, const Info& g, const std::vector<Member>& members, const Show& sh,
                         const Texts& t);
std::vector<Words> words(const Layout& L, const Info& g, const std::vector<Member>& members, const More& more,
                         const Show& sh, const Texts& t);

// 3.0.19-beta.1 (the testers: «не видно значок ГІ твоеї»): the guild's emblem from the game's own file
// (<client>\_tmpEmblem\<server>_<guild id>_<emblem id>.ebm, written by uaRO.exe's FUN_0063aab0 from the packet
// 0x0152: the BMP zlib-packed as the server keeps it; a plain BMP taken too) - side x side ARGB, its magenta
// (255, 0, 255, the game's key) see-through, made bigger pixel for pixel; empty when it is not one
std::vector<uint32_t> emblemPixels(const std::string& file, int side);

}  // namespace uagfx::guildcard
