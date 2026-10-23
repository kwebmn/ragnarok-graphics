#include "core/roext.hpp"

#include <cctype>
#include <map>

namespace uagfx::roext {
namespace {

std::string lower(std::string s) {
    for (char& c : s) c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim(const std::string& s) {
    const size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    return s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}

// [ROExt]'s keys (lower case) and values, the first of each, as Windows' profile functions find them
std::map<std::string, std::string> roextKeys(const std::string& text) {
    std::map<std::string, std::string> out;
    bool in = false;
    size_t at = 0;
    while (at <= text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = trim(text.substr(at, end - at));
        at = end + 1;
        if (!line.empty() && line[0] == '[') {
            const size_t close = line.find(']');
            in = close != std::string::npos && lower(trim(line.substr(1, close - 1))) == "roext";
            continue;
        }
        const size_t eq = line.find('=');
        if (!in || eq == std::string::npos) continue;
        out.emplace(lower(trim(line.substr(0, eq))), trim(line.substr(eq + 1)));
    }
    return out;
}

// GetPrivateProfileInt: the default for a key not there or empty, else the number it starts with (0 for none)
int profileInt(const std::map<std::string, std::string>& keys, const char* key, int fallback) {
    const auto it = keys.find(lower(key));
    if (it == keys.end() || it->second.empty()) return fallback;
    const std::string& v = it->second;
    size_t i = 0;
    const bool minus = v[0] == '-';
    if (v[0] == '-' || v[0] == '+') ++i;
    long long n = 0;
    for (; i < v.size() && std::isdigit(static_cast<unsigned char>(v[i])) && n < 0x7FFFFFFF; ++i) n = n * 10 + (v[i] - '0');
    return int(minus ? -n : n);
}

std::string num(int v) { return std::to_string(v); }
std::string flag(bool v) { return v ? "1" : "0"; }

constexpr uint32_t kLocked = 0x860A0000u;    // WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_SYSMENU | min box
constexpr uint32_t kCaptioned = 0x06CA0000u;  // WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | min box

}  // namespace

Settings fromDinput8Ini(const std::string& text) {
    const std::map<std::string, std::string> k = roextKeys(text);
    Settings s;
    s.minimizeKey = profileInt(k, "MinimizeKey", 0) != 0;
    s.mouseButtons = profileInt(k, "RemapMouseButtons", 1) != 0;
    s.altF4 = profileInt(k, "RemapAltF4", 0) != 0;
    s.windowOnTop = profileInt(k, "WindowOnTop", 0) != 0;
    s.windowLock = profileInt(k, "WindowLock", 1) != 0;
    s.windowX = profileInt(k, "WindowPosX", 0);
    s.windowY = profileInt(k, "WindowPosY", 0);
    s.windowWidth = (profileInt(k, "WindowWidth", 0) + 3) & ~3;
    s.windowHeight = profileInt(k, "WindowHeight", 0);
    const int cpu = profileInt(k, "AutoFreeCPU", 1);
    s.freeCpu = cpu == 0 ? 0 : cpu <= 1 ? 250 : cpu;
    s.codePage = profileInt(k, "CodePage", -1);
    return s;
}

std::vector<settings::IniEdit> iniEdits(const Settings& s) {
    return {{"roext", "window_lock", flag(s.windowLock)},     {"roext", "window_on_top", flag(s.windowOnTop)},
            {"roext", "window_x", num(s.windowX)},            {"roext", "window_y", num(s.windowY)},
            {"roext", "window_width", num(s.windowWidth)},    {"roext", "window_height", num(s.windowHeight)},
            {"roext", "mouse_buttons", flag(s.mouseButtons)}, {"roext", "alt_f4", flag(s.altF4)},
            {"roext", "minimize_key", flag(s.minimizeKey)},   {"roext", "free_cpu", num(s.freeCpu)},
            {"roext", "code_page", num(s.codePage)}};
}

bool isGameWindow(uint32_t exStyle, uint32_t style, bool parent, bool menu, bool param) {
    if (exStyle != 0 || parent || menu || param) return false;
    return (style & ~0x00080000u) == 0x02C20000u || style == 0x80020000u;  // the system menu or not; full screen
}

uint32_t styleOf(const Settings& s) { return s.windowLock ? kLocked : kCaptioned; }

WindowPlan windowPlan(const Settings& s, const int workArea[4], int frameWidth, int frameHeight) {
    WindowPlan p;
    p.style = styleOf(s);
    p.exStyle = s.windowOnTop ? 0x8u : 0u;  // WS_EX_TOPMOST
    if (s.windowWidth && s.windowHeight) {  // ROExt's DllMain: its size goes past the lock's work area
        p.place = true;
        p.x = s.windowX;
        p.y = s.windowY;
        p.width = ((s.windowWidth + 3) & ~3) + frameWidth;
        p.height = s.windowHeight + frameHeight;
    } else if (s.windowLock) {
        p.place = true;
        p.x = workArea[0];
        p.y = workArea[1];
        p.width = (workArea[2] - workArea[0] + 3) & ~3;
        p.height = workArea[3] - workArea[1];
    }
    return p;
}

bool transform(Message& m, const Settings& s, bool ctrlDown) {
    constexpr uint32_t kKeyDown = 0x100, kKeyUp = 0x101, kSysKeyDown = 0x104, kSysKeyUp = 0x105;
    constexpr uint32_t kF4 = 0x73, kScroll = 0x91, kSnapshot = 0x2C;
    switch (m.message) {
        case kKeyDown:
            if (s.minimizeKey && m.wParam == 'W' && ctrlDown) {
                m.message = 0x112;  // WM_SYSCOMMAND
                m.wParam = 0xF020;  // SC_MINIMIZE
                m.lParam = -1;
                return true;
            }
            return m.wParam != kScroll;
        case kKeyUp: return m.wParam != kScroll;
        case kSysKeyDown:
        case kSysKeyUp:
            if (!s.altF4) return true;
            if (m.wParam == kF4) {
                m.message ^= 4;  // Alt+F4 an F4 of the game's
            } else if (m.wParam == kSnapshot) {
                m.message = kSysKeyDown;  // Alt+Print Screen the Alt+F4 that closes it
                m.wParam = kF4;
            }
            return true;
        case 0x207:  // WM_MBUTTONDOWN, -UP, WM_XBUTTONDOWN, -UP
        case 0x208:
        case 0x20B:
        case 0x20C: {
            if (!s.mouseButtons) return true;
            const bool middle = m.message <= 0x208, down = (m.message & 1) != 0;
            m.wParam = middle ? 0x87u : (m.wParam >> 16) == 2 ? 0x85u : 0x86u;  // F24; X2 F22, X1 F23
            m.message = down ? kKeyDown : kKeyUp;
            return true;
        }
        default: return true;
    }
}

}  // namespace uagfx::roext
