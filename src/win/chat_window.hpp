#pragma once
#include <windows.h>

#include "core/frame.hpp"
#include "core/game_packets.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::chatwin {

// 3.0.11-beta.41 (stage 8 of the new interface; the owner: «делай чат»): [interface] chat - the chat redrawn at the
// bottom left (core/chat_panel). Its lines: what the game pushes into its own chat - the windows' manager's message 1
// (FUN_004db310: the words and their colour; message 0x14 the same), hooked at its entry, told apart by its caller
// (the packets' handlers of the chat near, the party's, the guild's, whispers, announcements); the battle's from the
// packets. The game's chat window (UI window 1, g_windowMgr + 0x84) keeps working out of sight - moved off the
// screen, its edit still taking the keys (the IME's words): its words, caret, the whisper's name and the send mode
// (DAT_00772d60) shown in the new one's input row, a click there giving it the keys. On the game's thread.
void install();
void packet(const packets::Event& e);
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// F10 its height, Alt+F10 folded to its input, Tab the channel - while it shows
bool key(UINT m, WPARAM wp, LPARAM lp);
// 3.0.11-beta.98: the game's chat line holds no words (the NPC talk's keys 1-9, Enter, Esc are its own then)
bool inputEmpty();
// Its box in the target while it shows (the hotbar keeps clear of it); false: not shown
bool panelBox(int& x, int& y, int& w, int& h);
// 3.0.11-beta.53: just before the game draws its windows (win/windows_draw) - its own chat kept out of sight
void beforeGameDraw();

}  // namespace uagfx::win::chatwin
