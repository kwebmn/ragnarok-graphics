#include "win/several_windows.hpp"

#include "core/layout.hpp"
#include "core/window_places.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win {
namespace {

// A window the player moves: with its caption, neither minimised nor maximised (a locked or full-screen one is not)
bool movable(HWND w) {
    const LONG style = GetWindowLongA(w, GWL_STYLE);
    return (style & WS_CAPTION) == WS_CAPTION && !IsIconic(w) && !IsZoomed(w);
}

std::wstring widened(const std::string& text, unsigned codePage) {
    const int n = MultiByteToWideChar(codePage, 0, text.data(), int(text.size()), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w(size_t(n), L'\0');
    MultiByteToWideChar(codePage, 0, text.data(), int(text.size()), &w[0], n);
    return w;
}

std::string placesFile() { return layout::join(gameDir(), layout::kWindowPlaces); }

}  // namespace

void SeveralWindows::check(HWND window, const std::string& name, const Config& config) {
    if (!window) return;
    // the game's text in the code page [roext] sets for it, else Windows' own
    const unsigned cp = config.clientRoext && config.roext.codePage > 0 ? unsigned(config.roext.codePage) : CP_ACP;
    title(window, config.windowTitleName ? name : "", cp);
    place(window, config.windowRememberPlace ? name : "");
}

void SeveralWindows::title(HWND window, const std::string& name, unsigned codePage) {
    if (name == titled_) return;
    if (!ownKept_) {
        wchar_t own[256] = {};
        InternalGetWindowText(window, own, 256);  // no message to the game's window procedure
        own_ = own;
        ownKept_ = true;
    }
    std::wstring text = own_;
    if (!name.empty()) text = widened(name, codePage) + (own_.empty() ? L"" : L" — " + own_);
    // past the game's own window procedure: Unicode whatever the window is, and nothing of the game's runs on it
    DefWindowProcW(window, WM_SETTEXT, 0, LPARAM(text.c_str()));
    titled_ = name;
}

void SeveralWindows::place(HWND window, const std::string& name) {
    RECT r{};
    if (!GetWindowRect(window, &r)) return;
    const places::Lookup lookup = [](const std::string& who) -> std::optional<places::Place> {
        std::string text;
        readTextFile(placesFile(), text);
        const places::Places all = places::parse(text);
        const auto it = all.find(who);
        if (it == all.end()) return std::nullopt;
        return it->second;
    };
    const places::Step s =
        places_.step(name, movable(window), places::Place{r.left, r.top}, GetTickCount64(), lookup);
    if (s.what == places::Step::Do::Move) {  // the character came: its window to its place, when it is on a monitor
        const RECT top{s.place.x, s.place.y, s.place.x + (r.right - r.left), s.place.y + 32};
        if (!MonitorFromRect(&top, MONITOR_DEFAULTTONULL)) {
            LOG_INFO("windows: the character's place %d,%d is on no monitor now - the window stays", s.place.x,
                     s.place.y);
            return;
        }
        SetWindowPos(window, nullptr, s.place.x, s.place.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        GetWindowRect(window, &r);
        places_.at(places::Place{r.left, r.top});
        LOG_INFO("windows: the character's window to its place %ld,%ld", r.left, r.top);
    } else if (s.what == places::Step::Do::Keep) {  // moved by the player: kept with the other windows' places
        std::string text;
        readTextFile(placesFile(), text);
        places::Places all = places::parse(text);
        all[name] = s.place;
        if (writeFileAtomic(placesFile(), places::format(all)))
            LOG_INFO("windows: the character's window kept at %d,%d", s.place.x, s.place.y);
    }
}

}  // namespace uagfx::win
