#include "win/own_windows.hpp"

#include <string>

#include "core/layout.hpp"
#include "win/console.hpp"
#include "win/log.hpp"
#include "win/options_card.hpp"
#include "win/paths.hpp"
#include "win/recap_window.hpp"
#include "win/reference_window.hpp"
#include "win/tracker.hpp"

namespace uagfx::win::ownwin {
namespace {

closedwin::Set& set() {
    static closedwin::Set s;
    static bool read = false;
    if (!read) {
        read = true;
        std::string text;
        if (readTextFile(layout::join(gameDir(), layout::kClosedWindows), text)) s = closedwin::read(text);
        if (s.any()) LOG_INFO("windows: closed by ✕ before - %s", closedwin::write(s).c_str());
    }
    return s;
}

closedwin::Switches& switches() {
    static closedwin::Switches on;
    return on;
}

unsigned& skipChar() {
    static unsigned c = 0;
    return c;
}

}  // namespace

bool closed(closedwin::Window w) { return closedwin::has(set(), w); }

void setClosed(closedwin::Window w, bool closed, const char* why) {
    closedwin::Set& s = set();
    if (closedwin::has(s, w) == closed) return;
    closedwin::set(s, w, closed);
    LOG_INFO("windows: %s %s (%s)", closedwin::keyOf(w), closed ? "closed" : "opened", why);
    const std::string path = layout::join(gameDir(), layout::kClosedWindows);
    if (!writeFileAtomic(path, closedwin::write(s))) LOG_INFO("windows: %s cannot be written", path.c_str());
}

void setSwitches(const closedwin::Switches& on) { switches() = on; }

std::vector<closedwin::Own> row() { return closedwin::row(switches()); }

bool lit(closedwin::Own o) {
    using closedwin::Own;
    switch (o) {
        case Own::Reference: return referencewin::isOpen();
        case Own::Hunt: return !closed(closedwin::Window::Hunt);
        case Own::Mvp: return !closed(closedwin::Window::Mvp);
        case Own::Tracker: return !trackerwin::hiddenNow();
        case Own::Party: return !closed(closedwin::Window::Party);
        case Own::Recap: return recapwin::isOpen();
        default: return false;
    }
}

void press(closedwin::Own o, const char* why) {
    using closedwin::Own;
    switch (o) {
        case Own::Reference: referencewin::toggleFromPad(); break;
        case Own::Hunt: toggle(closedwin::Window::Hunt, why); break;
        case Own::Mvp: toggle(closedwin::Window::Mvp, why); break;
        case Own::Tracker: trackerwin::toggleHidden(why); break;
        case Own::Party: toggle(closedwin::Window::Party, why); break;
        case Own::Recap: recapwin::toggle(why); break;
        case Own::Settings:  // 3.0.18-beta.122: «Налаштування» (the console while the new options' look is off)
            if (!optionscardwin::toggleOpen(why)) Console::askOpen();
            break;
        default: break;
    }
}

bool key(UINT m, WPARAM wp, LPARAM) {
    if ((m == WM_CHAR || m == WM_SYSCHAR) && skipChar() && wp == skipChar()) {
        skipChar() = 0;
        return true;
    }
    if (m != WM_KEYDOWN || GetKeyState(VK_CONTROL) >= 0 || GetKeyState(VK_MENU) < 0 || GetKeyState(VK_SHIFT) < 0)
        return false;
    closedwin::Own o{};
    if (!closedwin::ownOfKey(unsigned(wp), o)) return false;
    const std::vector<closedwin::Own> shown = row();
    bool on = false;
    for (const closedwin::Own x : shown) on = on || x == o;
    if (!on) return false;  // its switch off: the key stays the game's
    press(o, closedwin::keyName(o));
    skipChar() = unsigned(wp) - 'A' + 1;  // Ctrl+letter's own character
    return true;
}

}  // namespace uagfx::win::ownwin
