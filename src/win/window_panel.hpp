#pragma once
#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/game_windows.hpp"
#include "core/window_panel.hpp"

namespace uagfx::win {

// 3.0.3-beta.7 (the user: «в игре мышкой переключатся», «делай современно», «панельку ... можно было двигать»,
// «кнопка быстрая показать спрятать», «у каждого свое разрешение екрана»): the game windows' panel - a layered window
// owned by the game window and over it, never activated by a click (the game stays in front), its shapes by
// core/window_panel and its words by GDI. Shown in the main game window while two or more run; dragged by its top
// row (written to [windows] panel_place), hidden by its cross or Ctrl+Alt+0 ([windows] panel_hidden). On the game's
// thread: its window's messages come through the game's own loop. In the screen's real pixels (win/dpi).
class WindowPanel {
public:
    ~WindowPanel();
    // Every frame: shown or not, at its place, at the game's interface scale x [windows] panel_size
    void frame(HWND game, bool main, Config& config, int gameWidth);
    // The windows to show, a few times a second
    void update(const std::vector<gamewindows::Window>& all, uint32_t selfPid);
    void follow();  // the game window moved: over it again
    void toggle();  // Ctrl+Alt+0
    bool on() const { return config_ && config_->windowPanel; }  // [windows] panel: Ctrl+Alt+0 is its while on
    std::function<void(int number)> onPick;  // a card clicked: that window to the front

private:
    static LRESULT CALLBACK proc(HWND w, UINT m, WPARAM wp, LPARAM lp);
    LRESULT message(UINT m, WPARAM wp, LPARAM lp);
    bool create(HWND game);
    void redraw();
    POINT target() const;  // its place on the screen
    void hide(bool hidden);
    void keepPlace();

    HWND window_ = nullptr, game_ = nullptr;
    Config* config_ = nullptr;
    std::vector<gamewindows::Window> all_;
    uint32_t self_ = 0;
    std::string shownData_;  // what the cards say, as last drawn
    panel::Geometry geometry_;
    panel::Layout layout_ = panel::Layout::Row;
    float scale_ = 0;
    int roomW_ = 0, roomH_ = 0;
    POINT origin_{};  // the game window's client area on the screen
    panel::Place place_;
    std::string placeText_ = "?";  // [windows] panel_place as last read
    bool shown_ = false, dirty_ = true, everShown_ = false, panelOn_ = true, seenSetting_ = false;
    int hover_ = -1;
    bool hoverClose_ = false, tracking_ = false;
    int pressed_ = -2;  // pressed on: -1 the cross, a card's index, -2 nothing
    bool dragging_ = false;
    POINT dragFrom_{}, dragStart_{};
};

}  // namespace uagfx::win
