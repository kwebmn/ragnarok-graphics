#pragma once
#include <windows.h>

#include <cstdint>
#include <functional>
#include <string>

#include "core/config.hpp"
#include "core/game_windows.hpp"
#include "win/game_windows.hpp"
#include "win/window_panel.hpp"

namespace uagfx::win {

// 3.0.4-beta.4 (the user's idea 8): this game's character fell, was hit, was whispered to (win/notifier.cpp from the
// game's packets) - carried in its entry for the other windows' panels
namespace windowmarks {
void died();
void hit();
void whispered();
}  // namespace windowmarks

// 3.0.3-beta.6 (the user: «я хочу в игре мышкой переключатся и быстрой кнопкой»): this game's window among the
// player's others - its entry in the block they share (its number, the character, when it was last in front), and the
// keys that switch the windows ([windows] switch_keys), taken in the game window's procedure before the game sees
// them: Ctrl+Alt+digit is a chat macro in uaRO.exe. 3.0.3-beta.9 (the user: switching «более гладко», «блымает»,
// «нагрузку фоновых окон надо уменьшить»): a switch by the keys or the panel asks the next window to get its picture
// whole behind this one and shows it then (core/game_windows kSummonWaitMs at most); on its full screen the game no
// longer minimises itself when the focus goes to another game window (the blink), as it still does for the desktop or
// another program; a window wholly behind another game window draws 2 frames a second. On the game's thread.
class WindowSwitch {
public:
    WindowSwitch();
    ~WindowSwitch();
    // A few times a second: the entry written, the window's procedure ours once, the panel's windows, whether this
    // window is wholly behind another game window
    void check(HWND window, bool foreground, const std::string& name, const Config& config);
    // Every frame once its scale is known: the panel; a switch waiting for its window's picture. fullScreen: the
    // game on its full screen (DDSCL_FULLSCREEN) - it minimises itself when the focus goes
    void frame(HWND window, bool main, Config& config, double k, int targetWidth, int targetHeight, bool fullScreen);
    void draw(FrameRecord& record, gpu::Renderer& renderer) { panel_.draw(record, renderer); }
    // Every frame: asked by the window in front to get its picture whole - true while it draws whole for it
    bool summoned(bool foreground);
    // After each present: whole (not light) - the third whole one answers the ask
    void presented(bool light);
    bool hidden() const { return hidden_; }  // wholly behind another game window of the player's
    // The game window's messages: true - taken, the game does not get them (`result` the answer)
    bool key(unsigned vk, LPARAM lParam);
    bool mouse(HWND window, UINT m, LPARAM lp, LRESULT& result) { return panel_.mouse(window, m, lp, result); }
    bool activate(HWND window, WPARAM wp);  // WM_ACTIVATE on the full screen
    // 3.0.3-beta.11 (the user: «счетчик кадров замер»): the game window no longer in front, in any mode - the
    // driver's windows over it hidden at once (on its full screen the game no longer presents behind another one)
    std::function<void()> onLeave;
    // 3.0.3-beta.12: while it is behind, ten times a second - what the game's presents did, they may have stopped
    std::function<void()> onBehind;
    bool timer(HWND window, WPARAM id);     // WM_TIMER: where the focus went from the full screen

private:
    void publish(bool frontNow);
    void act(const gamewindows::KeyAction& a);
    void to(int number, const char* by);
    void show(int number, const char* by, bool ready, uint64_t waited);

    SharedWindows shared_;
    WindowPanel panel_;
    HWND window_ = nullptr;
    WindowKeys keys_ = WindowKeys::All;
    gamewindows::Entry mine_{};
    bool fullScreen_ = false, hidden_ = false;
    int pendingNumber_ = 0;  // a switch waiting for its window's whole picture
    int32_t pendingSerial_ = 0;
    uint64_t pendingSince_ = 0;
    const char* pendingBy_ = "";
    int32_t seenSummon_ = 0, summonSerial_ = 0;  // asked: the last ask taken, the one drawn for
    uint64_t summonedAt_ = 0;
    int wholeFrames_ = 0;
    uint64_t leaveSince_ = 0;  // the focus gone from the full screen: where to, decided by the timer
};

}  // namespace uagfx::win
