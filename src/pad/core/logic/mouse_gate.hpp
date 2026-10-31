#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace uapad {

// Which of the real mouse's buttons and wheel reach the client. DirectInput gives the client the mouse
// wherever the cursor is while the game window is active: over another monitor, the title bar, a window on
// top. A press counts only when it starts over the game; a drag that started there goes on outside.
class MouseGate {
public:
    struct Out {
        uint8_t buttons = 0;  // bit i: rgbButtons[i] passes
        bool wheel = false;
    };
    // A button that goes down this soon after the game window came to the front is the click that brought it.
    static constexpr uint32_t kActivationMs = 300;

    // Whether the game window is in front now. Call it on every mouse poll and every tick: the polls stop
    // while the game is behind, so a tick sees it go and the next poll sees it come back. Going behind
    // forgets the held buttons.
    void watch(bool foreground, uint32_t nowMs);
    // `buttons`: bit i = rgbButtons[i] down. `overGame`: the Windows cursor is over the game's client area and
    // nothing covers it there. The click that brought the window to the front stays out unless `focusClick`.
    Out filter(uint8_t buttons, bool overGame, bool focusClick, uint32_t nowMs);
    // 3.0.11-beta.33: a press a part of the driver's took in its own window message - out until it is let go,
    // whatever is under the cursor at the next poll (the part may be gone by then: its close button)
    void claim(uint8_t buttons);
    void reset();  // forget the held buttons

private:
    uint8_t passing_ = 0;  // went down over the game: pass until let go
    uint8_t blocked_ = 0;  // went down elsewhere, or activated the window: hidden until let go
    bool foreground_ = false;
    std::optional<uint32_t> frontSinceMs_;  // the last time the window came to the front
};

// The middle and the two side mouse buttons as keys F1-F9 for the client.
class MouseKeys {
public:
    struct KeyEvent {
        int key;  // 1-9: F1-F9
        bool down;
    };
    // `held`: bit 0 middle, bit 1 the 4th, bit 2 the 5th button. `keys`: the F-key of each, 0 for none.
    // `allowed`: a press may start a key now (the game is in front, the menu closed, the cursor over the game).
    // `foreground`: the game window is in front; when it is not, every held key is let go.
    std::vector<KeyEvent> update(uint8_t held, const std::array<int, 3>& keys, bool allowed, bool foreground);
    std::vector<KeyEvent> releaseAll();

private:
    std::array<int, 3> holds_{};  // the key each button holds down now, 0 for none
    uint8_t prevHeld_ = 0;
};

// How long the message pump sleeps now: `sleepMs` while the client's queue is empty and the game window is
// behind another one or minimized, else 0.
int backgroundSleepMs(bool queueEmpty, bool foreground, bool minimized, int sleepMs);

// A part of the driver's own in the game's picture (the windows' panel, the answer card, the new minimap,
// «Довідник»): its clicks are not the game's (3.0.3-beta.8), and 3.0.11-beta.24 the world under it does not
// react to the game's cursor, as under the game's own windows. Its rectangle in the game window's client pixels
// (Windows' size of them), right and bottom outside; all: the whole window is its now (the panel dragged, its
// menu open).
struct Blocked {
    bool on = false;
    bool all = false;
    int left = 0, top = 0, right = 0, bottom = 0;
};
// The part over a point of the game window's client area (the first of them), -1 for none.
int blockedAt(const Blocked* parts, int count, int x, int y);
// 3.0.11-beta.108: windows over each other - the parts in `first` looked at before the rest (the window stack's top
// first: a click or a drop on two windows is the one's on top)
int blockedAtOrdered(const Blocked* parts, int count, const std::vector<int>& first, int x, int y);
// The same for the game's own cursor: x, y of its width x height picture in a client area of clientW x clientH.
int blockedAtGame(const Blocked* parts, int count, int clientW, int clientH, int x, int y, int width, int height,
                  const std::vector<int>& first = {});

}  // namespace uapad
