#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::npctalk {

// 3.0.11-beta.98 (the owner: «доделаем окна интерфейсов и сделаем поддержку в них геймпада»; the concept's «Разговор
// с NPC»): the NPC talk in style A in place of the game's text and menu windows - the NPC's name (a script's «[Name]»
// first line) and where, the server's words with their ^RRGGBB colours wrapped to the window, the menu's options
// numbered in two columns, the button the game's window waits for («Далі», «Закрити»), and under them the keys or the
// controller's buttons; the text untranslated, as the server sends it. The parts that need no Windows; the words by
// GDI (win/npc_talk).

struct Run {
    std::string text;  // UTF-8
    uint32_t rgb = 0;
};
// A line's runs by its ^RRGGBB colours; black and the near black (the game's window was white) take `base`, the dark
// ones are lit so they read on the dark glass
std::vector<Run> colours(const std::string& utf8, uint32_t base);

enum class Button : uint8_t { None, Next, Close };
struct Talk {
    std::string name, place;  // «Kafra Employee», «prontera»
    std::vector<std::string> lines, options;
    int selected = -1;  // the menu's option the game's list has
    Button button = Button::None;
};
Talk named(Talk t);  // the name from a «[Name]» first line, that line out of the words

struct Texts {
    std::string next, close;   // «Далі», «Закрити»
    std::string keys, server;  // «1–8 вибір · Enter далі · Esc закрити», «Текст — як у сервера»
    std::string choose, ok;    // the controller's: «вибір», «OK»
};

using Measure = std::function<float(const std::string& utf8, float px, bool bold)>;

struct Show {
    int hoverOption = -1;
    bool hoverButton = false, hoverClose = false;
    bool pad = false, sony = false;
};

struct Placed {  // a run of words placed in the window
    panel::Box box;
    std::string text;
    uint32_t rgb = 0;
};

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;
    panel::Box glass, title, close, body, button, footer;
    std::vector<Placed> lines;
    std::vector<panel::Box> options;
    std::vector<panel::Box> padGlyphs, padWords;
};
// Its bottom middle at (centreX, bottom) in the target
Layout layout(int centreX, int bottom, float scale, const Talk& t, const Show& sh, const Texts& tx, const Measure& m);

enum class Part : uint8_t { None, Close, Button, Option, Title, Body };
struct Hit {
    Part part = Part::None;
    int index = -1;
};
Hit hitTest(const Layout& L, float x, float y);

void paint(panel::Picture& out, const Layout& L, const Talk& t, const Show& sh);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;
    uint32_t rgb = 0xEEF2F8;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Talk& t, const Show& sh, const Texts& tx, const Measure& m);

}  // namespace uagfx::npctalk
