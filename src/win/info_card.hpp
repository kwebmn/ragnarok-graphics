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
enum class Kind : uint8_t { Item, Skill };
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
// 3.0.18-beta.138: an item's slots as the game's itemslotcounttable.txt has them (0 none)
int slotsOf(int id);

}  // namespace uagfx::win::infocardwin
