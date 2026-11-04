#pragma once
#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

#include "core/frame.hpp"
#include "core/window_panel.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::emotewin {

// 3.0.11-beta.205 (the tester's third list: «alt l - список емоцій ... в старому стилі», «по даблкліку теж
// виконувались»): [interface] emotes - the game's emotion list (Alt+L: UI window 0x57 at g_windowMgr + 0x1AC, class
// 0x69C320, SendMsg FUN_004521d0: message 6 / 0x87 closes) as a card (core/emote_card): its commands from its own
// list (+ 0x6C: 49 words, msgstringtable's), their pictures the game's (emotion.spr by emotion_index3.act, an action
// each); a click does what the game's list does (+ 0x60 the emotion, FUN_00452540: into the chat's line, or the
// macros' field while that window is open), a double click sends the chat's line after it as Enter does (the chat
// window's SendMsg 6 / 0x76: its «/» commands go to the game's own parser); the game's window hidden while the card
// shows. On the game's thread.
void frame(HWND game, bool on, unsigned codePage, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
bool shown();
bool close();  // Esc: its close
void beforeGameDraw();
bool box(int& x, int& y, int& w, int& h);
// 3.0.11-beta.214 (the chat wheel too): the list's 49 pictures at `side` px in its order (made once a size; empty
// ones when the game's files are not read); an emotion command's number in the list (msgstringtable's lines
// 0x220-0x23D, 0x304-0x30F, 0x3B1-0x3B7 as the game's list fills itself), -1 none. On the game's thread.
const std::vector<panel::Picture>& emotionPictures(int side);
int emotionNumber(const std::string& command);

}  // namespace uagfx::win::emotewin
