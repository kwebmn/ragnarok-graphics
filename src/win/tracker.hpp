#pragma once
#include <windows.h>

#include <string>
#include <vector>

#include "core/frame.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win::trackerwin {

// 3.0.11-beta.70 ([interface] tracker; the concept's «держать перед глазами» - «Отслеживание — что закрепить (квест,
// путь, вещь для сбора), выбирает игрок; окно стоит левее столбика баффов»): the tracker panel left of the buffs'
// column (core/tracker_panel) - the quests pinned by «Відстежувати» in their cards (what to bring, whom to give it),
// the way the new minimap leads, the items pinned to collect (who drops them most). ▾ folds it, ✕ hides it until a new
// pin; a click on an entry's name opens its card, its ✕ lets it go. The pins kept in uaro-graphics\tracker-state.txt.
void frame(HWND game, bool on, double k, int targetWidth, int targetHeight);
void draw(FrameRecord& record, gpu::Renderer& renderer);
bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result);
// The pins («Довідник»'s cards): another pin shows the panel again
void toggleQuest(const std::string& slug);
void toggleItem(int id);
const std::vector<std::string>& quests();
const std::vector<int>& items();

}  // namespace uagfx::win::trackerwin
