#pragma once
#include <windows.h>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::answercard {

// 3.0.9-beta.4 (the user: «делай все» - the support canvas' screen 2): the developer's answer in the game. The game's
// watcher keeps the answers not seen yet in uaro-graphics\reports\answers.json (core/report_answers); they show as a
// card at the top centre of the game's picture (core/answer_card, drawn as the windows' panel: every renderer) while
// the game is in front. «Відкрити» - a click on the card - starts uaro-graphics.exe --answer R-XXXXXX («Звернення» on
// that conversation); «Пізніше» puts it off. Behind other windows or minimised, a Windows notification tells of new
// ones instead ([notify] report_answer). On the game's thread.
// 3.0.20-beta.17 (the owner: «чтобы не задалбывало постоянно вверху сообщение в игре что надо ответить», «я жму
// контрол шифт ф10 а оно ... предлагает прочитать»): one card for all the new answers (answers::cardOf: the one the
// developer waits on first, «ще N»), gone by itself after a few seconds unless the developer waits for an answer;
// opened, put off or gone, they stay put off between games until a newer one (uaro-graphics\reports\answers-later.txt);
// one Windows notification for them; Ctrl+Shift+F10 is always «Нове звернення».

// Every frame once its scale is known (k: the target's pixels a game pixel)
void frame(HWND game, double k, int targetWidth, int targetHeight);
// The game behind others, ten times a second (it may present no more): the file read, the new answers told
void behind(HWND game);
// Into the frame's record before it goes (the windows' panel's slot): the card over the game's interface
void draw(FrameRecord& record, gpu::Renderer& renderer);
// A message of the game window's mouse: true - the card's, the game does not get it (`result` its answer)
bool mouse(HWND game, UINT m, LPARAM lp, LRESULT& result);

}  // namespace uagfx::win::answercard
