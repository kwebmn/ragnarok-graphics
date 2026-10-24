#pragma once
#include <windows.h>

#include <cstdint>
#include <string>

#include "core/config.hpp"
#include "core/game_windows.hpp"
#include "win/game_windows.hpp"
#include "win/window_panel.hpp"

namespace uagfx::win {

// 3.0.3-beta.6 (the user: «я хочу в игре мышкой переключатся и быстрой кнопкой»): this game's window among the
// player's others - its entry in the block they share (its number, the character, when it was last in front), and the
// keys that switch the windows ([windows] switch_keys), taken in the game window's procedure before the game sees
// them: Ctrl+Alt+digit is a chat macro in uaRO.exe. On the game's thread.
class WindowSwitch {
public:
    WindowSwitch();
    ~WindowSwitch();
    // A few times a second: the entry written, the window's procedure ours once, the panel's windows
    void check(HWND window, bool foreground, const std::string& name, const Config& config);
    // 3.0.3-beta.8: every frame once its scale is known - the windows' panel (win/window_panel) in the main game
    // window's picture; and into the frame's record before it goes
    void frame(HWND window, bool main, Config& config, double k, int targetWidth, int targetHeight) {
        panel_.frame(window, main, config, k, targetWidth, targetHeight);
    }
    void draw(FrameRecord& record, gpu::Renderer& renderer) { panel_.draw(record, renderer); }
    // A key going down in the game window: true - taken (the game does not get it)
    bool key(unsigned vk, LPARAM lParam);
    bool mouse(HWND window, UINT m, LPARAM lp, LRESULT& result) { return panel_.mouse(window, m, lp, result); }

private:
    void publish(bool frontNow);
    void act(const gamewindows::KeyAction& a);
    void to(int number, const char* by);

    SharedWindows shared_;
    WindowPanel panel_;
    HWND window_ = nullptr;
    WindowKeys keys_ = WindowKeys::All;
    gamewindows::Entry mine_{};
};

}  // namespace uagfx::win
