#pragma once
#include <windows.h>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::answercard {

// 3.0.9-beta.4 (the user: «делай все» - the support canvas' screen 2): the developer's answer in the game. The game's
// watcher keeps the answers not seen yet in uaro-graphics\reports\answers.json (core/report_answers); the newest one
// not opened or put off in this game shows as a card at the top centre of the game's picture (core/answer_card, drawn
// as the windows' panel: every renderer) while the game is in front. «Відкрити» - a click on the card, or
// Ctrl+Shift+F10 while it shows - starts uaro-graphics.exe --answer R-XXXXXX («Звернення» on that conversation);
// «Пізніше» puts it off until the next game. Behind other windows or minimised, a Windows notification tells of each
// new one instead ([notify] report_answer). On the game's thread.

// Every frame once its scale is known (k: the target's pixels a game pixel)
void frame(HWND game, double k, int targetWidth, int targetHeight);
// The game behind others, ten times a second (it may present no more): the file read, the new answers told
void behind(HWND game);
// Into the frame's record before it goes (the windows' panel's slot): the card over the game's interface
void draw(FrameRecord& record, gpu::Renderer& renderer);
// A message of the game window's mouse: true - the card's, the game does not get it (`result` its answer)
bool mouse(HWND game, UINT m, LPARAM lp, LRESULT& result);
// Ctrl+Shift+F10: true - the card was shown and its answer opens (no «Нове звернення» then)
bool openByKey();

}  // namespace uagfx::win::answercard
