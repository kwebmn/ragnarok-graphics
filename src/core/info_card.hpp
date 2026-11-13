#pragma once
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/npc_talk.hpp"
#include "core/window_panel.hpp"

namespace uagfx::infocard {

// 3.0.11-beta.149 (the owner: «сделай все без остановки, в инлайн режиме» - the game's windows still in the old look,
// the item's and the skill's cards first): the game's item card (UI window 0xC) and skill card (0x2E) in style A - the
// icon and the name in the title, the game's description from its own table under them (its ^RRGGBB colours made to
// read on the dark glass, its white spacers hidden, its line breaks kept), an item's cards in its slots, an item's big
// picture beside the words when the client has one; the wheel scrolls a long one. The parts that need no Windows; the
// words by GDI (win/info_card).

// The game's description tables: «1201#», its lines, «#» (items); «MG_FIREWALL#» alike (skills) - a key to its lines;
// the «//» lines and the CRs left out
std::map<std::string, std::vector<std::string>> descriptions(const std::string& text);
// 3.0.11-beta.270 (the tester: «у скіла Cure ... криво виводяться прохідні скіли» - «4Skill Requirements : Heal 2»):
// a description's lines a class's own - its job's number before the colour («4^777777Skill Requirements ...», the
// client's way for a skill several classes have): the character's job's kept without the number, the others' dropped;
// none for the job - each with its class's name (`name` of the job, "" unknown) before it
std::vector<std::string> forJob(const std::vector<std::string>& lines, int job,
                                const std::function<std::string(int)>& name);
// 3.0.11-beta.200 (the tester: «скіл вивчено, а замість його опису пише що не вивчено»): the game's skill card keeps
// the skill's name as shown (FUN_00505d60: skillnametable's «Coin_Flip» spaced, «Coin Flip»), not its AEGIS name;
// the card's skill: the character's one of that name (its AEGIS name and level), else the table's (not learned),
// else the words themselves (an AEGIS name as before 3.0.11-beta.200 had it)
struct LearnedSkill {
    std::string aegis;
    int level = 0;
};
LearnedSkill skillOfCard(const std::string& shown, const std::vector<LearnedSkill>& session,
                         const std::map<std::string, std::string>& names);
// itemslotcounttable.txt: «1101#3#» - an item's number to its slots
std::map<int, int> slotCounts(const std::string& text);
// The folder of data\ the game reads its language's tables from, by Windows' language (uaRO.exe 0x4EF2xx, its primary
// id): «german\», «italian\», «portuguese\», «turkish\», else «english\» (Ukrainian and Russian among them)
std::string languageFolder(unsigned langId);

struct Card {
    std::string title;               // «+7 Blade [4]», «Fire Wall»
    std::string note;                // under it: «×38», «Lv 10», «Не опізнано»; "" none
    std::vector<std::string> lines;  // the description, UTF-8 with the game's ^RRGGBB
    std::vector<std::string> cards;  // an item's slots: a card's name, "" an empty slot
    // 3.0.18-beta.109: each slot's card picture (its item icon; null none), by the slot
    std::vector<const panel::Picture*> cardIcons;
    // 3.0.18-beta.110 (the owner: «окно карты как в оригинале»): the picture a card's illustration - big, in the
    // middle, over the words (the game's card window)
    bool art = false;
    const panel::Picture* icon = nullptr;     // at iconSide(scale), premultiplied
    const panel::Picture* picture = nullptr;  // the big picture, fitted into pictureRoom (premultiplied)
};
int iconSide(float scale);
void pictureRoom(float scale, int& w, int& h);
void artRoom(float scale, int& w, int& h);  // 3.0.18-beta.110: a card's illustration's room
// An image (straight alpha) fitted into w x h with its sides in proportion, premultiplied; none: an empty picture
panel::Picture fitted(const std::vector<uint32_t>& argb, int width, int height, int w, int h);

struct Texts {
    std::string slots;  // «Слоти»
    std::string empty;  // «порожньо»
    std::string reference;  // 3.0.18-beta.242: «У довіднику ›» under it all - its item's card there; "" none
};
struct Show {
    int scroll = 0;  // the first row shown
    bool hoverClose = false;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box glass, title, note, close, icon, picture, body, slotsLabel;
    panel::Box reference;  // 3.0.18-beta.242: the «У довіднику ›» link (w 0 none)
    std::vector<npctalk::Placed> lines;  // every row's runs, their y as if not scrolled
    int rows = 0, shownRows = 0;
    float lineH = 0;
    std::vector<panel::Box> slots;
};
// Its top left at (x, y) of the target, kept on its targetW x targetH
Layout layout(const Card& c, int x, int y, int targetW, int targetH, float scale, const Show& sh, const Texts& t,
              const npctalk::Measure& m);
enum class Part : uint8_t { None, Close, Body, Title, Reference };
Part hitTest(const Layout& L, float x, float y);  // the picture's pixels
// 3.0.18-beta.109 (the owner: «как было в оригинале»): the slot under (x, y), -1 none - a card there opens its own
// description (the game's item window opens a card's window from its slot)
int slotAt(const Layout& L, float x, float y);
int scrolled(const Layout& L, int scroll, int notches);  // the wheel's: three rows a notch (down positive), kept within


enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Card& c, const Show& sh, const Texts& t);

}  // namespace uagfx::infocard
