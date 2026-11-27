#pragma once
#include <cstdint>
#include <string>

#include "core/settings/strings.hpp"

namespace uagfx::itemui {

// 3.0.24-beta.20 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, «Общие детали»; the plan
// docs/superpowers/plans/2026-11-27-item-windows-old-layout.md, task 1): what every item window shares - its keys'
// hint under it, the weight's (and the places') bar, the head with its count, the letters its words may hold. The
// look: the theme's classes «.uw …» (core/ui_theme_css).
enum class Window : uint8_t {
    Items, Equip, Storage, Cart, ShopBuy, ShopSell, Vend, VendSetup, MyShop, Trade, Amount, Card, Pick,
    Count  // how many there are
};
settings::Text hintOf(Window w);  // the line of keys under the window

// A bar's fill: its whole per cent (0-100) and its tone - "ok", "warn" from 70 %, "full" (red) from 90 %
struct Fill {
    int percent = 0;
    const char* tone = "ok";
};
Fill fillOf(int64_t used, int64_t most);
std::string countTitle(const std::string& name, int used, int most);  // «Речі 16/100»

// R-WP985Q («непонятные символы в карточке»): every letter of `utf8` is one the interface's font has (IBM Plex Sans,
// both faces: core/plex_widths.inc) - stars, crosses and triangles are drawn, not written. False on a broken byte.
bool fontHas(const std::string& utf8);

// 3.0.24-beta.21 (the spec's «Візок»: «Відкрити лавку»): the cart's footer button opens a shop by Vending (MC_VENDING)
// when the character has it, nothing is picked (their «take» first) and no shop is open or being made
constexpr uint16_t kVendingSkill = 41;
bool offersShop(int vendingLevel, int picked, bool shopNow);

}  // namespace uagfx::itemui
