#include "win/mcp_input.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "pad/win/remote.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win::mcpinput {
namespace {

std::atomic<ULONGLONG> g_hoverUntil{0};  // 3.0.22-beta.40: a hover's end (0 none)

mcp::CallResult said(const std::string& text, bool error = false) {
    mcp::CallResult r;
    r.isError = error;
    r.content.push_back(mcp::Content::textOf(text));
    return r;
}

std::string pair(long a, long b) { return std::to_string(a) + "," + std::to_string(b); }

int send(INPUT in) { return int(SendInput(1, &in, sizeof(INPUT))); }

INPUT keyInput(int vk, bool up, bool extended) {
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    in.ki.wVk = WORD(vk);
    in.ki.wScan = WORD(MapVirtualKeyA(UINT(vk), MAPVK_VK_TO_VSC));  // DirectInput reads the scan code
    in.ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0u) | (extended ? KEYEVENTF_EXTENDEDKEY : 0u);
    return in;
}

INPUT mouseInput(DWORD flags, LONG dx = 0, LONG dy = 0) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dwFlags = flags;
    in.mi.dx = dx;
    in.mi.dy = dy;
    return in;
}

// 3.0.22-beta.36 (20.11: the owner moved his mouse during the clicks - two went where his hand put the cursor): the
// press only while the cursor is still where it was put, over the game's own window (not one above it), the game in
// front
bool stillThere(HWND game, POINT p) {
    POINT now{};
    if (!ready(game).empty() || !GetCursorPos(&now) || std::labs(now.x - p.x) > 1 || std::labs(now.y - p.y) > 1)
        return false;
    DWORD pid = 0;
    const HWND under = WindowFromPoint(p);
    if (under) GetWindowThreadProcessId(under, &pid);
    return under && pid == GetCurrentProcessId();  // the game's window or one of ours over it
}

const char* kMoved = "no click made: the mouse moved (the player's hand?), another window over the place or the game "
                     "not in front any more - ask again when the mouse is left alone";

// 3.0.22-beta.37 (the owner: «я хочу чтобы клиент мог работать в фоне, когда ты проверки делаешь»): from inside the
// game - its window may stay behind others, the player's mouse and keyboard untouched. The game's DirectInput mouse
// polled through the pad's wrapper (pad/win/remote) is what it needs.
std::string insideNot(HWND window) {
    if (!window || !IsWindow(window)) return "no game window yet";
    if (IsIconic(window))
        return "the game's window is minimised - keys and clicks need it shown (behind other windows is fine)";
    if (!uapad::remote::polled()) return "the game's mouse is not read through uaRO Pad here (an old dinput.dll?)";
    return "";
}

bool sendWait(HWND w, UINT m, WPARAM wp, LPARAM lp) {
    DWORD_PTR answer = 0;
    return SendMessageTimeoutA(w, m, wp, lp, SMTO_ABORTIFHUNG, 1000, &answer) != 0;
}

// The game's cursor where (cx, cy) of the window lands in its picture
bool gamePoint(const Game& g, int cx, int cy, int clientW, int clientH, long& ex, long& ey) {
    if (g.width <= 0 || g.height <= 0 || clientW <= 0 || clientH <= 0) return false;
    ex = std::lround(double(cx) * g.width / double(clientW));
    ey = std::lround(double(cy) * g.height / double(clientH));
    return true;
}

mcp::CallResult keyInside(const Game& g, const mcp::KeyCombo& k, const std::string& words) {
    if (g_hoverUntil.exchange(0)) uapad::remote::setMouse(uapad::remote::Mouse{});  // a hover ends
    const UINT scan = MapVirtualKeyA(UINT(k.vk), MAPVK_VK_TO_VSC);
    const UINT down = k.alt ? WM_SYSKEYDOWN : WM_KEYDOWN, up = k.alt ? WM_SYSKEYUP : WM_KEYUP;
    uapad::remote::setMods(k.ctrl, k.shift, k.alt);  // the game's GetAsyncKeyState and its thread's key state
    Sleep(10);
    bool posted =
        PostMessageA(g.window, down, WPARAM(k.vk), LPARAM(mcp::keyMessageBits(int(scan), k.extended, k.alt, false)));
    Sleep(80);  // the game takes it at its next frame, the modifiers held for it
    const LPARAM let = LPARAM(mcp::keyMessageBits(int(scan), k.extended, k.alt, true));
    posted = PostMessageA(g.window, up, WPARAM(k.vk), let) && posted;
    Sleep(60);
    uapad::remote::setMods(false, false, false);
    LOG_INFO("mcp: key %s inside the game%s", words.c_str(), posted ? "" : " - not posted");
    if (!posted) return said("the key could not be told to the game's window", true);
    return said("pressed " + words + " inside the game (its window may stay behind others) - look with `screenshot`");
}

mcp::CallResult clickInside(const Game& g, double x, double y, int width, bool right, bool twice) {
    RECT c{};
    GetClientRect(g.window, &c);  // this thread's points, as the game's thread and the pad count them
    int cx = 0, cy = 0;
    if (!mcp::clickPlace(x, y, width, int(c.right), int(c.bottom), cx, cy))
        return said("the place is past the picture: a screenshot " + std::to_string(width) +
                        " wide shows the window's " + pair(c.right, c.bottom) + " points",
                    true);
    long ex = 0, ey = 0;
    const bool known = gamePoint(g, cx, cy, int(c.right), int(c.bottom), ex, ey);
    g_hoverUntil = 0;  // a hover ends: the click's own from here
    uapad::remote::setMouse(uapad::remote::Mouse{true, cx, cy, 0});
    int gx = -1, gy = -1;
    for (int i = 0; i < 30; ++i) {  // the game's cursor driven there, 0.45 s at most
        if (known && readGameCursor(gx, gy) && std::labs(gx - ex) <= 2 && std::labs(gy - ey) <= 2) break;
        Sleep(15);
    }
    const LPARAM at = MAKELPARAM(cx, cy);
    PostMessageA(g.window, WM_MOUSEMOVE, 0, at);
    Sleep(30);
    const bool swapped = GetSystemMetrics(SM_SWAPBUTTON) != 0;  // the device's buttons, as the claims count them
    const uint8_t device = uint8_t(right != swapped ? 2 : 1);
    bool told = true;
    for (int i = 0; i < (twice ? 2 : 1); ++i) {
        const UINT downMsg = right ? (i ? WM_RBUTTONDBLCLK : WM_RBUTTONDOWN) : (i ? WM_LBUTTONDBLCLK : WM_LBUTTONDOWN);
        // the window's message first, answered: a part of the driver's that takes it claims the press before the
        // game's mouse shows it (then the game never sees it)
        told = sendWait(g.window, downMsg, right ? MK_RBUTTON : MK_LBUTTON, at) && told;
        uapad::remote::setMouse(uapad::remote::Mouse{true, cx, cy, device});
        Sleep(70);  // a few of the game's frames see it held
        uapad::remote::setMouse(uapad::remote::Mouse{true, cx, cy, 0});
        told = sendWait(g.window, right ? WM_RBUTTONUP : WM_LBUTTONUP, 0, at) && told;
        Sleep(60);
    }
    Sleep(60);
    uapad::remote::setMouse(uapad::remote::Mouse{});
    std::string where;
    if (known && readGameCursor(gx, gy)) {
        where = "; the game's cursor at " + pair(gx, gy) + " of its " + std::to_string(g.width) + "x" +
                std::to_string(g.height) + " picture";
        if (std::labs(gx - ex) > 3 || std::labs(gy - ey) > 3)
            where += " - not at the place (" + pair(ex, ey) + "): the click may have gone elsewhere";
    }
    const std::string what = std::string(right ? "right" : "left") + (twice ? " double click" : " click");
    LOG_INFO("mcp: %s inside the game at %s of a %d-wide screenshot = %s in the window%s%s", what.c_str(),
             pair(long(x), long(y)).c_str(), width, pair(cx, cy).c_str(), told ? "" : " (a message not answered)",
             where.c_str());
    return said(what + " inside the game at " + pair(long(x), long(y)) + " = " + pair(cx, cy) + " in the window" +
                where + (told ? "" : " (the window did not answer a message in time)") + " - look with `screenshot`");
}

mcp::CallResult keyFront(const Game& g, const mcp::KeyCombo& k, const std::string& words) {
    std::vector<int> mods;
    if (k.ctrl) mods.push_back(VK_CONTROL);
    if (k.shift) mods.push_back(VK_SHIFT);
    if (k.alt) mods.push_back(VK_MENU);
    int sent = 0;
    size_t held = 0;
    for (int m : mods) {
        sent += send(keyInput(m, false, false));
        ++held;
        Sleep(15);
    }
    if (!ready(g.window).empty()) {  // 3.0.22-beta.36: the game left the front meanwhile - its keys let go, no more
        for (size_t i = held; i-- > 0;) send(keyInput(mods[i], true, false));
        LOG_INFO("mcp: key %s - not pressed, the game not in front any more", words.c_str());
        return said("no key pressed: the game is not in front any more", true);
    }
    sent += send(keyInput(k.vk, false, k.extended));
    Sleep(70);  // held for a few frames: the game reads its keys' state each frame
    sent += send(keyInput(k.vk, true, k.extended));
    for (auto m = mods.rbegin(); m != mods.rend(); ++m) {
        Sleep(15);
        sent += send(keyInput(*m, true, false));
    }
    const int want = int(mods.size()) * 2 + 2;
    LOG_INFO("mcp: key %s - %d of %d key events taken", words.c_str(), sent, want);
    if (sent != want)
        return said("Windows took " + std::to_string(sent) + " of " + std::to_string(want) +
                        " key events - a window of a program run as administrator in front?",
                    true);
    return said("pressed " + words + " - look with `screenshot`");
}

mcp::CallResult clickFront(const Game& g, double x, double y, int width, bool right, bool twice) {
    RECT c{};
    GetClientRect(g.window, &c);
    int cx = 0, cy = 0;
    if (!mcp::clickPlace(x, y, width, int(c.right), int(c.bottom), cx, cy))
        return said("the place is past the picture: a screenshot " + std::to_string(width) +
                        " wide shows the window's " + pair(c.right, c.bottom) + " pixels",
                    true);
    POINT p{cx, cy};
    ClientToScreen(g.window, &p);
    // the move as a mouse's (the game in full screen follows its cursor by DirectInput's moves), then the cursor put
    // exactly there
    const int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int vw = std::max(2, GetSystemMetrics(SM_CXVIRTUALSCREEN));
    const int vh = std::max(2, GetSystemMetrics(SM_CYVIRTUALSCREEN));
    send(mouseInput(MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK,
                    LONG(std::lround(double(p.x - vx) * 65535.0 / double(vw - 1))),
                    LONG(std::lround(double(p.y - vy) * 65535.0 / double(vh - 1)))));
    SetCursorPos(p.x, p.y);
    Sleep(90);  // a few frames: the game's cursor there, what it points at lit
    const DWORD down = right ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_LEFTDOWN;
    const DWORD up = right ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_LEFTUP;
    int sent = 0;
    for (int i = 0; i < (twice ? 2 : 1); ++i) {
        if (i) Sleep(70);
        if (!stillThere(g.window, p)) {  // 3.0.22-beta.36
            POINT now{};
            GetCursorPos(&now);
            LOG_INFO("mcp: a click at %s not made (press %d): the cursor at %s, the game %s", pair(p.x, p.y).c_str(),
                     i + 1, pair(now.x, now.y).c_str(), ready(g.window).empty() ? "in front" : "not in front");
            return said(i ? std::string("the first click made, the second not: ") + kMoved : kMoved, true);
        }
        sent += send(mouseInput(down));
        Sleep(60);
        sent += send(mouseInput(up));
    }
    Sleep(120);  // the game's frames take the click before the answer
    std::string where;
    int gx = 0, gy = 0;
    long ex = 0, ey = 0;
    if (gamePoint(g, cx, cy, int(c.right), int(c.bottom), ex, ey) && readGameCursor(gx, gy)) {
        where = "; the game's cursor at " + pair(gx, gy) + " of its " + std::to_string(g.width) + "x" +
                std::to_string(g.height) + " picture";
        if (std::labs(gx - ex) > 3 || std::labs(gy - ey) > 3)
            where += " - not at the place (" + pair(ex, ey) + "): the click may have gone elsewhere";
    }
    const std::string what = std::string(right ? "right" : "left") + (twice ? " double click" : " click");
    LOG_INFO("mcp: %s at %s of a %d-wide screenshot = %s in the window (%d of %d button events)%s", what.c_str(),
             pair(long(x), long(y)).c_str(), width, pair(cx, cy).c_str(), sent, twice ? 4 : 2, where.c_str());
    if (sent != (twice ? 4 : 2))
        return said("Windows took " + std::to_string(sent) + " button events of " + std::to_string(twice ? 4 : 2),
                    true);
    return said(what + " at " + pair(long(x), long(y)) + " = " + pair(cx, cy) + " in the window" + where +
                " - look with `screenshot`");
}

}  // namespace

mcp::CallResult hover(const Game& g, double x, double y, int width, int seconds) {
    const std::string inside = insideNot(g.window);
    if (!inside.empty()) return said(inside, true);
    RECT c{};
    GetClientRect(g.window, &c);
    int cx = 0, cy = 0;
    if (!mcp::clickPlace(x, y, width, int(c.right), int(c.bottom), cx, cy))
        return said("the place is past the picture: a screenshot " + std::to_string(width) +
                        " wide shows the window's " + pair(c.right, c.bottom) + " points",
                    true);
    uapad::remote::setMouse(uapad::remote::Mouse{true, cx, cy, 0});
    g_hoverUntil = GetTickCount64() + ULONGLONG(std::clamp(seconds, 1, 30)) * 1000;
    PostMessageA(g.window, WM_MOUSEMOVE, 0, MAKELPARAM(cx, cy));
    long ex = 0, ey = 0;
    int gx = -1, gy = -1;
    const bool known = gamePoint(g, cx, cy, int(c.right), int(c.bottom), ex, ey);
    for (int i = 0; i < 30; ++i) {  // the game's cursor there, 0.45 s at most
        if (known && readGameCursor(gx, gy) && std::labs(gx - ex) <= 2 && std::labs(gy - ey) <= 2) break;
        Sleep(15);
    }
    LOG_INFO("mcp: hover inside the game at %s of a %d-wide screenshot = %s, %d s", pair(long(x), long(y)).c_str(),
             width, pair(cx, cy).c_str(), seconds);
    return said("the game's cursor held at " + pair(long(x), long(y)) + " = " + pair(cx, cy) + " in the window for " +
                std::to_string(seconds) + " s - look with `screenshot`");
}

void tick() {
    const ULONGLONG until = g_hoverUntil;
    if (until && GetTickCount64() >= until && g_hoverUntil.exchange(0)) uapad::remote::setMouse(uapad::remote::Mouse{});
}

std::string ready(HWND window) {
    if (!window || !IsWindow(window)) return "no game window yet";
    if (IsIconic(window)) return "the game's window is minimised - keys and clicks need it shown and in front";
    if (GetForegroundWindow() != window)
        return "the game's window is not in front - keys and clicks are real input and go to the window in front; "
               "the game must be brought in front (a click on it) first";
    return "";
}

mcp::CallResult key(const Game& g, const mcp::KeyCombo& k, const std::string& words) {
    const std::string inside = insideNot(g.window);
    if (inside.empty()) return keyInside(g, k, words);
    const std::string why = ready(g.window);  // without the pad's mouse: real input, the game in front
    if (!why.empty()) return said(inside + "; " + why, true);
    return keyFront(g, k, words);
}

mcp::CallResult drag(const Game& g, double x, double y, double x2, double y2, int width) {
    const std::string inside = insideNot(g.window);
    if (!inside.empty()) return said("no drag made: " + inside, true);
    RECT c{};
    GetClientRect(g.window, &c);
    int ax = 0, ay = 0, bx = 0, by = 0;
    if (!mcp::clickPlace(x, y, width, int(c.right), int(c.bottom), ax, ay) ||
        !mcp::clickPlace(x2, y2, width, int(c.right), int(c.bottom), bx, by))
        return said("a place is past the picture: a screenshot " + std::to_string(width) + " wide shows the window's " +
                        pair(c.right, c.bottom) + " points",
                    true);
    g_hoverUntil = 0;
    const uint8_t device = uint8_t(GetSystemMetrics(SM_SWAPBUTTON) != 0 ? 2 : 1);
    uapad::remote::setMouse(uapad::remote::Mouse{true, ax, ay, 0});
    long ex = 0, ey = 0;
    const bool known = gamePoint(g, ax, ay, int(c.right), int(c.bottom), ex, ey);
    int gx = -1, gy = -1;
    for (int i = 0; i < 30; ++i) {  // the game's cursor there first, 0.45 s at most
        if (known && readGameCursor(gx, gy) && std::labs(gx - ex) <= 2 && std::labs(gy - ey) <= 2) break;
        Sleep(15);
    }
    PostMessageA(g.window, WM_MOUSEMOVE, 0, MAKELPARAM(ax, ay));
    Sleep(30);
    bool told = sendWait(g.window, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(ax, ay));
    uapad::remote::setMouse(uapad::remote::Mouse{true, ax, ay, device});
    Sleep(90);
    constexpr int kSteps = 16;  // a frame or two a step: the windows' drags follow the cursor as a hand's would
    for (int i = 1; i <= kSteps; ++i) {
        const int px = ax + (bx - ax) * i / kSteps, py = ay + (by - ay) * i / kSteps;
        uapad::remote::setMouse(uapad::remote::Mouse{true, px, py, device});
        PostMessageA(g.window, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(px, py));
        Sleep(35);
    }
    Sleep(90);
    uapad::remote::setMouse(uapad::remote::Mouse{true, bx, by, 0});
    told = sendWait(g.window, WM_LBUTTONUP, 0, MAKELPARAM(bx, by)) && told;
    Sleep(80);
    uapad::remote::setMouse(uapad::remote::Mouse{});
    LOG_INFO("mcp: drag inside the game from %s to %s of a %d-wide screenshot = %s to %s in the window%s",
             pair(long(x), long(y)).c_str(), pair(long(x2), long(y2)).c_str(), width, pair(ax, ay).c_str(),
             pair(bx, by).c_str(), told ? "" : " (a message not answered)");
    return said("dragged inside the game from " + pair(long(x), long(y)) + " to " + pair(long(x2), long(y2)) +
                (told ? "" : " (the window did not answer a message in time)") + " - look with `screenshot`");
}

mcp::CallResult wheel(const Game& g, double x, double y, int width, int notches, bool ctrl) {
    const std::string inside = insideNot(g.window);
    if (!inside.empty()) return said("no wheel turned: " + inside, true);
    RECT c{};
    GetClientRect(g.window, &c);
    int cx = 0, cy = 0;
    if (!mcp::clickPlace(x, y, width, int(c.right), int(c.bottom), cx, cy))
        return said("the place is past the picture", true);
    g_hoverUntil = 0;
    uapad::remote::setMouse(uapad::remote::Mouse{true, cx, cy, 0});
    Sleep(120);  // the game's cursor there first
    if (ctrl) uapad::remote::setMods(true, false, false);
    Sleep(20);
    POINT screen{cx, cy};
    ClientToScreen(g.window, &screen);  // the wheel's place is the screen's
    const int step = notches > 0 ? 1 : -1;
    bool told = true;
    for (int i = 0; i != notches; i += step) {
        const WPARAM wp = MAKEWPARAM(ctrl ? MK_CONTROL : 0, uint16_t(int16_t(step * WHEEL_DELTA)));
        told = sendWait(g.window, WM_MOUSEWHEEL, wp, MAKELPARAM(screen.x, screen.y)) && told;
        Sleep(40);
    }
    if (ctrl) uapad::remote::setMods(false, false, false);
    uapad::remote::setMouse(uapad::remote::Mouse{});
    LOG_INFO("mcp: wheel %+d%s inside the game at %s", notches, ctrl ? " with Ctrl" : "", pair(cx, cy).c_str());
    return said("turned the wheel " + std::to_string(notches) + (ctrl ? " with Ctrl" : "") + " at " +
                pair(long(x), long(y)) + (told ? "" : " (the window did not answer in time)") +
                " - look with `screenshot`");
}

mcp::CallResult click(const Game& g, double x, double y, int width, bool right, bool twice) {
    const std::string inside = insideNot(g.window);
    if (inside.empty()) return clickInside(g, x, y, width, right, twice);
    const std::string why = ready(g.window);
    if (!why.empty()) return said(inside + "; " + why, true);
    return clickFront(g, x, y, width, right, twice);
}

}  // namespace uagfx::win::mcpinput
