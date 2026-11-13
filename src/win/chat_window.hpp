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
// 3.0.11-beta.240: [interface] chat_fade - idle this many seconds, its ground fades (its tabs and input stay); 0 never
void setFade(int seconds);
void setLog(bool on);  // 3.0.13-beta.3: [interface] chat_log
void setLongLines(bool on);  // 3.0.13-beta.12: [interface] long_chat
// 3.0.13-beta.3: [interface] chat_party, chat_guild, chat_own, chat_others - the lines coming from now in them
void setColours(const std::string& party, const std::string& guild, const std::string& own, const std::string& others);
// 3.0.11-beta.241: a press on its corner - its own (its size), not the interface's drag of the whole chat
bool ownsPress(HWND game, LPARAM lp);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// F10 its height, Alt+F10 folded to its input, Tab the channel - while it shows
// 3.0.11-beta.284 (the owner's feature 17): Ctrl+F in the chat's field - search its lines (before «Довідник»'s)
bool searchKey(UINT m, WPARAM wp, LPARAM lp);
bool key(UINT m, WPARAM wp, LPARAM lp);
// 3.0.11-beta.98: the game's chat line holds no words (the NPC talk's keys 1-9, Enter, Esc are its own then)
bool inputEmpty();
// 3.0.18-beta.208 (the walk mode): the game's focus is the chat's words or name field, or no field at all
bool focusInChat();
void clearLine();  // the words' field emptied (the walk mode's Esc)
bool gameBattle();  // the game's battle mode (/bm) on
// 3.0.18-beta.134 (the pad analysis: chat on the controller): the channel round (near, party, guild - as Tab), the
// history scrolled `by` lines (up +)
void padChannel();
// 3.0.18-beta.147: a whisper to this one (UTF-8) - its name in the game's field, the words' field takes the keys
bool whisperTo(const std::string& utf8Name);
// 3.0.18-beta.242 (the reference's Shift+click on a name): `utf8` («[Poring]») after the chat line's words (a space
// between), the line given the keys; false: no chat line, or the line would be longer than it takes
bool putIntoLine(const std::string& utf8);
void padScroll(int by);
// Its box in the target while it shows (the hotbar keeps clear of it); false: not shown
bool panelBox(int& x, int& y, int& w, int& h);
// 3.0.11-beta.53: just before the game draws its windows (win/windows_draw) - its own chat kept out of sight
void beforeGameDraw();

}  // namespace uagfx::win::chatwin
