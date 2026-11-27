#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace uagfx::themechoice {

// 3.0.24-beta.19 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, the themes): what Alt+O's rows of
// the interface's look show - the theme as two pictures (dark, light), the accent's chips with their colours, and
// «свій колір»'s palette (3 rows x 10 hues) with its field «#RRGGBB»
bool swatchRow(const std::string& key);  // interface.theme alone
std::vector<uint32_t> palette();         // uitheme::paletteColour row by row: 30
// The palette's cell (row, col) as [interface] accent and accent_colour: {"own", "#RRGGBB"}
std::pair<std::string, std::string> pick(int row, int col);
// A colour typed into the field ("#c86bff", "C86BFF", " #C86BFF ") as accent_colour's "#RRGGBB"; "" not a colour
std::string typed(const std::string& text);
// The accent's chip's colour dot for its word (auto: none; own: the own colour when set) - "#rrggbb", "" none
std::string dot(const std::string& word, const std::string& ownColour);

}  // namespace uagfx::themechoice
