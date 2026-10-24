#pragma once
#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/frame.hpp"
#include "core/game_windows.hpp"
#include "core/window_panel.hpp"

namespace uagfx::gpu {
class Renderer;
}

namespace uagfx::win {

// 3.0.3-beta.7 (the user: «в игре мышкой переключатся», «делай современно», «панельку ... можно было двигать»,
// «кнопка быстрая показать спрятать», «у каждого свое разрешение екрана»); 3.0.3-beta.8 in the game's picture (the
// user: «в самой игре это самое то что я хотел»): the game windows' panel - its shapes by core/window_panel and its
// words by GDI into a picture of the target's pixels, sent with the frame as a texture and one quad over the game's
// interface, so every renderer draws it (the 64-bit one too). Shown in the main game window while two or more run.
// The game window's mouse over it is the panel's: a click on a card brings that window to the front, its top row
// drags it ([panel] place), the button beside its cross lays the cards the other way ([panel] layout), the cross or
// Ctrl+Alt+0 hides it ([panel] hidden); the pad's mouse gate keeps those clicks from the game. 3.0.3-beta.10 (the
// user: «при нажатии правой кнопкой мышки ... опции разные задавать»): the right button on it opens its menu at the
// cursor (core/window_panel menuRows), a second picture over it - a click on a row changes that setting at once and in
// the file; a click off it, Esc, «hide», the window going behind or the panel gone close it. 3.0.3-beta.11: the icons
// (core/window_panel Layout::Icons) with the name of the one under the mouse in a tip, a third picture over the panel;
// the game's cursor moved to the frame's end, over them all (R-45N3WS). On the game's thread.
class WindowPanel {
public:
    // Every frame, once the frame's scale is known: shown or not, its picture and its place in the target's pixels -
    // the game's interface scale (k: the target's pixels a game pixel) x [panel] size
    void frame(HWND game, bool main, Config& config, double k, int targetWidth, int targetHeight);
    void update(const std::vector<gamewindows::Window>& all, uint32_t selfPid);  // a few times a second
    // Into the frame's record before it goes: the picture when it changed, the quad over the game's picture
    void draw(FrameRecord& record, gpu::Renderer& renderer);
    // A message of the game window's mouse: true - the panel's, the game does not get it (`result` its answer)
    bool mouse(HWND game, UINT m, LPARAM lp, LRESULT& result);
    void toggle();  // Ctrl+Alt+0
    bool closeMenu();  // Esc: true - its menu was open (closed now; the game does not get the key)
    bool on() const { return config_ && config_->panelShow; }  // [panel] show: Ctrl+Alt+0 is its while on
    std::function<void(int number)> onPick;  // a card clicked: that window to the front

private:
    void render();
    void hide(bool hidden);
    void keepPlace();
    void block(HWND game);  // the pad's gate told where the panel is
    bool toTarget(HWND game, int x, int y, float& tx, float& ty) const;
    panel::Hit hitAt(float tx, float ty) const;
    void openMenu(float tx, float ty);
    void renderMenu();  // its picture and its place: by the point it was opened at, inside the target
    void renderTip();   // 3.0.3-beta.11: the icon under the mouse - its name (and levels)
    void pick(panel::MenuHit h);

    Config* config_ = nullptr;
    std::vector<gamewindows::Window> all_;
    std::vector<gamewindows::Marks> marks_;  // 3.0.4-beta.4: all_'s, the other windows' characters
    uint32_t self_ = 0;
    std::string shownData_;  // what the cards say, as last drawn
    panel::Geometry geometry_;
    panel::Picture picture_;
    panel::Layout layout_ = panel::Layout::Row;
    float scale_ = 0, k_ = 1;  // the panel's scale; the target's pixels a game pixel
    int targetW_ = 0, targetH_ = 0, opacity_ = 0;
    int x_ = 0, y_ = 0;  // its top left in the target's pixels
    panel::Place place_;
    std::string placeText_ = "?";  // [panel] place as last read
    bool shown_ = false, dirty_ = true, upload_ = true, everShown_ = false, panelOn_ = true, seenSetting_ = false;
    int hover_ = -1;
    panel::Part hoverPart_ = panel::Part::None;
    panel::Hit pressed_;
    bool dragging_ = false;
    float dragFromX_ = 0, dragFromY_ = 0;
    int dragStartX_ = 0, dragStartY_ = 0;
    uint32_t texture_ = 0;
    int texW_ = 0, texH_ = 0;
    bool underCursor_ = false;  // 3.0.3-beta.9: drawn under the game's own cursor (found in the frame)
    int cursorSaid_ = -1, cursorLines_ = 0;  // 3.0.3-beta.12: the log told whether it was found (1, 0), how often
    // 3.0.3-beta.10: its menu - the rows as last drawn, where it was opened (the target's pixels), its top left
    bool menuOpen_ = false, menuDirty_ = false, menuUpload_ = false;
    std::vector<panel::MenuRow> menuRows_;
    panel::MenuGeometry menuGeometry_;
    panel::Picture menuPicture_;
    panel::MenuHit menuHover_, menuPressed_;
    float menuAtX_ = 0, menuAtY_ = 0, menuScale_ = 0;
    int menuX_ = 0, menuY_ = 0;
    uint32_t menuTexture_ = 0;
    int menuTexW_ = 0, menuTexH_ = 0;
    uint8_t swallowed_ = 0;  // the buttons (1 left, 2 right, 4 middle) whose press closed the menu: their let-go too
    bool rightOnPanel_ = false;  // the right button pressed on the panel: its let-go there opens the menu
    // 3.0.3-beta.11: the icons' tip - what it says as drawn ("" none), its picture
    std::string tipData_;
    panel::Picture tipPicture_;
    uint32_t tipTexture_ = 0;
    int tipTexW_ = 0, tipTexH_ = 0;
    bool tipUpload_ = false;
};

}  // namespace uagfx::win
