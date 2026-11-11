#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/window_panel.hpp"

namespace uagfx::enterveil {

// 3.0.11-beta.203 (the tester: «при логіні, коли скріпт швиденько проклікує вікна з персонажом ... я б взагалі
// спробував позбутися проклікування, або приховав це додатковим вікном завантаження»): while the login enters the game
// by itself (one click: the login, the character select, on to the map) the screen dark under a card - «Вхід у гру…»,
// the character's name and line, three dots running - till the map starts (its own loading picture then), a box asks
// (a refusal, an error), the login card is back, or 30 s. The parts that need no Windows; the words by GDI.
struct Texts {
    std::string title, name, line;
};
struct Layout {
    float s = 1;
    int x = 0, y = 0, w = 0, h = 0;  // the card's picture in the target
    panel::Box card, title, name, line, dots;
};
Layout layout(int targetW, int targetH, float scale);
  // the card; its dots lit up to `phase` (0-2)

enum class Align : uint8_t { Left, Right, Center };
struct Words {
    panel::Box box;
    std::string text;  // UTF-8
    uint32_t rgb = 0xEEF2F8;
    float px = 13;
    bool bold = false;
    Align align = Align::Center;
};
std::vector<Words> words(const Layout& L, const Texts& t);

enum class End : uint8_t { No, World, Box, Login, Late };
constexpr uint64_t kMostMs = 30000;
// 3.0.18-beta.164 (R-CFNPF6): `held` - the map entry's pause holds the character's selection (win/map_entry_hook):
// no 30 s then, the veil tells the time left
End ending(bool mapStarted, bool boxAsks, bool loginBack, uint64_t since, uint64_t now, bool held = false);
std::string clock(unsigned seconds);  // «2:27»

}  // namespace uagfx::enterveil
