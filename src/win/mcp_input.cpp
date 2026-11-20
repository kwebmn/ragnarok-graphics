#include "win/mcp_input.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win::mcpinput {
namespace {

mcp::CallResult said(const std::string& text, bool error = false) {
    mcp::CallResult r;
    r.isError = error;
    r.content.push_back(mcp::Content::textOf(text));
    return r;
}

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

std::string pair(long a, long b) { return std::to_string(a) + "," + std::to_string(b); }

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

}  // namespace

std::string ready(HWND window) {
    if (!window || !IsWindow(window)) return "no game window yet";
    if (IsIconic(window)) return "the game's window is minimised - keys and clicks need it shown and in front";
    if (GetForegroundWindow() != window)
        return "the game's window is not in front - keys and clicks are real input and go to the window in front; "
               "the game must be brought in front (a click on it) first";
    return "";
}

mcp::CallResult key(const Game& g, const mcp::KeyCombo& k, const std::string& words) {
    const std::string why = ready(g.window);
    if (!why.empty()) return said(why, true);
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

mcp::CallResult click(const Game& g, double x, double y, int width, bool right, bool twice) {
    const std::string why = ready(g.window);
    if (!why.empty()) return said(why, true);
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
    if (g.width > 0 && g.height > 0 && c.right > 0 && c.bottom > 0 && readGameCursor(gx, gy)) {
        const long ex = std::lround(double(cx) * g.width / double(c.right));
        const long ey = std::lround(double(cy) * g.height / double(c.bottom));
        where = "; the game's cursor at " + pair(gx, gy) + " of its " + std::to_string(g.width) + "x" +
                std::to_string(g.height) + " picture";
        if (std::labs(gx - ex) > 3 || std::labs(gy - ey) > 3)
            where += " - not at the place (" + pair(ex, ey) + "): the click may have gone elsewhere";
    }
    const std::string what = std::string(right ? "right" : "left") + (twice ? " double click" : " click");
    LOG_INFO("mcp: %s at %s of a %d-wide screenshot = %s in the window (%d of %d button events)%s", what.c_str(),
             pair(long(x), long(y)).c_str(), width, pair(cx, cy).c_str(), sent, twice ? 4 : 2, where.c_str());
    if (sent != (twice ? 4 : 2)) return said("Windows took " + std::to_string(sent) + " button events of " +
                                             std::to_string(twice ? 4 : 2), true);
    return said(what + " at " + pair(long(x), long(y)) + " = " + pair(cx, cy) + " in the window" + where +
                " - look with `screenshot`");
}

}  // namespace uagfx::win::mcpinput
