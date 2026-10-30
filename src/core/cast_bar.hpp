#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::castbar {

// 3.0.11-beta.48 (the concept's cast label; the owner on its version 4: «каст: над героем полосой, как в старом
// клиенте, но не как старый»): the character's cast over the hero - the skill's icon, its name and the seconds left
// on a pill (86 %), a gold bar under it where the game's own gauge stood (that one hidden while this shows). The parts
// that need no Windows; the words by GDI (win/cast_bar).
struct Data {
    std::string name;  // «Fire Bolt»; "" the skill not known (only the time)
    std::string left;  // «0,8 с»
    float done = 0;    // 0..1
    const panel::Picture* icon = nullptr;  // at iconSide(), premultiplied
};
int iconSide(float scale);

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box pill, icon, name, left, bar;
};
// Centred at centreX, its bar's top at barTop (the target's pixels)
Layout layout(const Data& d, int centreX, int barTop, float scale);
void paint(panel::Picture& out, const Layout& L, const Data& d);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 14;
    bool bold = true;
    Align align = Align::Left;
};
std::vector<Words> words(const Layout& L, const Data& d);

// The seconds left as the label shows them: tenths, rounded up ("0,8 с" for 750 ms; `unit` the language's)
std::string seconds(int ms, char decimal, const std::string& unit);

}  // namespace uagfx::castbar
