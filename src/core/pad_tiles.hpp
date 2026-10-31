#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::padtiles {

// 3.0.11-beta.90 (the concept «Контроллер в интерфейсе uaRO», stage 6): Start on the controller opens a tile of the
// game's windows in the middle of the screen - «Речі», «Характеристики», «Уміння», «Спорядження», «Група», «Карта»,
// «Довідник», the game's options and the controller's own menu, three to a row; the D-pad (or the left stick) moves the
// gold frame, A opens the one framed, B or Start closes the tile. The parts that need no Windows; the words by GDI.

constexpr int kTiles = 9;
enum class Tile : uint8_t { Items, Stats, Skills, Equip, Party, Map, Reference, Options, Pad };

struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the picture in the target
    panel::Box glass, title, hints;
    std::vector<panel::Box> tiles;
};
Layout layout(int centreX, int centreY, float scale);  // its middle at the screen's
int hitTest(const Layout& L, float x, float y);         // the tile there; -1 none
int moved(int at, int dx, int dy);                     // the frame moved round its row or its column

void paint(panel::Picture& out, const Layout& L, int focus, bool sony);

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xECEFF5;
    float px = 12;
    bool bold = false;
    Align align = Align::Left;
};
// `names` the tiles' in Tile's order
std::vector<Words> words(const Layout& L, const std::vector<std::string>& names, const std::string& title,
                         const std::string& open, const std::string& close, bool sony);

}  // namespace uagfx::padtiles
