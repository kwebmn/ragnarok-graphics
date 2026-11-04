#pragma once
#include <cstdint>

namespace uagfx::consolebox {

// 3.0.11-beta.209 (the tester: «ця панелька кліента - не влазить на маленьких екранах, треба зробити адаптивною,
// можливо щоб теж ставала більш прозорою разом з інтерфейсом»): the console's size by the game's window - two fifths
// across, three quarters down as before, its letters smaller in a small window (a 46th of its height, 12 px at least,
// the system's size at most) and all of it inside the window; its rows what is left after its title and its foot's
// lines (as GDI breaks them); its ground as see-through as the interface's panels ([interface] panel_opacity)
struct Box {
    int fontH = 15, line = 22;
    int x = 0, y = 0, w = 0, h = 0;  // in the game's window
};
Box box(int clientW, int clientH, int systemFontH);
int visibleRows(const Box& b, int footHeight);
uint8_t groundAlpha(int panelOpacity);

}  // namespace uagfx::consolebox
