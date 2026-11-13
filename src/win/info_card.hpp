#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::infocardwin {

// 3.0.11-beta.149 (the owner: «сделай все без остановки, в инлайн режиме»): [interface] cards - the game's item card
// (UI window 0xC at g_windowMgr + 0xC8, its class's vtable 0x698D88, SendMsg FUN_00478300: its ITEM_INFO at +0x60)
// and skill card (0x2E at + 0xD4, 0x698EB8, FUN_00479910: its skill's AEGIS name at +0x74) in the new look
// (core/info_card) beside the cursor that opened them; the game's windows stay made, their shown flags held at 0, our
// ✕ (or Esc, or the controller's B) presses their own close (message 6, 0x87). The descriptions from the game's own
// tables in the folder it reads them from (data\english\ for most). On the game's thread.
// 3.0.18-beta.139 (the owner: a slot's card opened over the thing's own words, not moved): Card - a slot's card in
// its own window beside the item card (either button on the slot), moved by its title, its own ✕
enum class Kind : uint8_t { Item, Skill, Card };
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(Kind kind, FrameRecord& record, gpu::Renderer& renderer);
bool mouse(Kind kind, HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool key(Kind kind, UINT m, WPARAM wp, LPARAM lp);  // ↑ ↓ PageUp PageDown scroll
bool shown(Kind kind);
void close(Kind kind);
void beforeGameDraw();  // the game's cards hidden
enum class PadCommand : uint8_t { Up, Down, Close };
void padCommand(Kind kind, PadCommand c);
bool box(Kind kind, int& x, int& y, int& w, int& h);
// 3.0.11-beta.230 («Навички»' side panel): a skill's description as the game's table has it, UTF-8 without its colour
// marks and without its first line when that is the name again; empty when the table has none
std::vector<std::string> skillDescription(const std::string& aegis, const std::string& name);
// 3.0.18-beta.242 (the reference's cards): an item's description as the game's table has it (the bag's card's), UTF-8
// without its colour marks; empty when the table has none
std::vector<std::string> itemDescription(int id);
// 3.0.18-beta.253 (R-WP985Q): the same with its colour marks (^RRGGBB) - «Weight: ^77777750^000000» tells a fact's
// value (core/item_desc reads it)
std::vector<std::string> itemDescriptionMarked(int id);
// 3.0.18-beta.138: an item's slots as the game's itemslotcounttable.txt has them (0 none)
int slotsOf(int id);
// 3.0.18-beta.142 (the owner: «все» - the windows' audit): a thing's item card by its ITEM_INFO in the game (a list
// node's +8) - the game's own card (UI window 0xC, its message 0x18), drawn in the new look; false: not made
bool showItem(uint32_t itemInfo);
// 3.0.18-beta.144: [interface] element_colours - a thing's element colour (core/item_elements; 0 none or off) by its
// number and its ITEM_INFO (its cards: a forged one's own, an armour card's); as "#rrggbb" ("" none)
void setElementColours(bool on);
int elementOf(int id, const int cards[4]);  // 3.0.18-beta.146: the element (0 none or the switch off)
uint32_t elementColour(int id, const uint8_t* itemInfo);
std::string elementHex(int id, const uint8_t* itemInfo);
// A thing's name made full and what is inside it, from its ITEM_INFO (72 bytes): its cards' words in the name, its
// slots after it («[3]»), its cards and free slots in `inside` (core vendshop::inside) - only for one identified
void describe(const uint8_t* itemInfo, int id, unsigned codePage, std::string& name, std::string& inside);
// 3.0.18-beta.145 (the tester: no slots seen - "Jur", the old client "Jur [3]"): the name with its
// slots as the game writes it, for one identified and not forged
std::string withSlots(const std::string& name, int id, const uint8_t* itemInfo);

}  // namespace uagfx::win::infocardwin
