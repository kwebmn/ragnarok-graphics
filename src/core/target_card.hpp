#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::targetcard {

// 3.0.11-beta.38 (stage 7 of the new interface, the HUD; the owner: «делай карточку цели»): the monster the
// character fights, in a short card at the screen's top middle, style A (the concept's target16, v19_cave) - its
// picture in a round, its name, «Lv 15», its element as a chip («Undead 1»), a book that opens its card in
// «Довідник»; pointed at - its race and size, its HP and experience, the elements that hurt it most and those that
// heal it. The parts that need no Windows; the words go over it by GDI (win/target_card).

struct Data {
    int mob = 0;
    std::string name;
    int level = 0;
    std::string element;  // «Undead 1»: its element's name (the game's, English) and level
    uint32_t elementRgb = 0x9AA3B5;
    std::string kind;     // «Undead · Medium · Boss»
    std::string numbers;  // «HP 2 772 · Base 1 002 · Job 643»
    std::string weak;     // «Fire 200 %, Holy 200 %» - the most hurting, from 150 %
    std::string heals;    // «Poison, Dark» - those that heal it
    const panel::Picture* picture = nullptr;  // its standing frame fitted into the round; none: a dark round
    // 3.0.11-beta.68 (the concept's «карточка цели со строкой «Ваша панель»»): the hotbar's skills' element
    // multipliers on it («Fire Bolt ×2 · Jupitel Thunder ×1», core/bar_hint); "" none
    std::string bar;
    // 3.0.11-beta.86 (the concept «Контроллер в интерфейсе uaRO», stage 2): played with the controller - the card's
    // end says its buttons (A attacks, the right stick another target), PlayStation's look
    bool pad = false, padSony = false;
};
struct Texts {
    std::string weak;   // «Слабкий до: {1}»
    std::string heals;  // «Лікують: {1}»
    std::string book;   // the book's tip: «Картка в довіднику»
    std::string bar;    // 3.0.11-beta.68: «Ваша панель: {1}»
    std::string attack, next;  // 3.0.11-beta.86: «Атака», «Інша ціль»
};

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box card, round, name, level, element, book;
    panel::Box hints;    // 3.0.11-beta.86: the controller's buttons at its end (w 0: none)
    panel::Box details;  // under the card while pointed at (w 0: none)
};
// Its top middle at (centreX, top) in the target; `details`: pointed at - its lines under it
Layout layout(const Data& d, const Texts& t, int centreX, int top, float scale, bool details);

enum class Part : uint8_t { None, Card, Book };
Part hitTest(const Layout& L, float x, float y);  // the picture's pixels

void paint(panel::Picture& out, const Layout& L, const Data& d, bool hoverBook);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Data& d, const Texts& t, bool hoverBook);

// The elements hurting a monster of `element` at `level` most (from 150 %, the most first, at most `most`) and those
// healing it, by the server's table (refpages::elementDamage): their numbers 0-9
struct Weakness {
    std::vector<std::pair<int, int>> weak;  // element, per cent
    std::vector<int> heals;
};
Weakness weakness(int element, int level, size_t most = 3);

}  // namespace uagfx::targetcard
