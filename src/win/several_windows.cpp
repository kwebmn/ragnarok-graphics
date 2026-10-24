#include "win/several_windows.hpp"

#include "core/client_patch.hpp"
#include "core/layout.hpp"
#include "core/window_places.hpp"
#include "core/window_title.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win {
namespace {

constexpr uint64_t kTitleMs = 1000;  // the numbers in the title at most once a second: the taskbar redraws its button

// A window the player moves: with its caption, neither minimised nor maximised (a locked or full-screen one is not)
bool movable(HWND w) {
    const LONG style = GetWindowLongA(w, GWL_STYLE);
    return (style & WS_CAPTION) == WS_CAPTION && !IsIconic(w) && !IsZoomed(w);
}

std::string placesFile() { return layout::join(gameDir(), layout::kWindowPlaces); }

// The client's name: its build's (uaro-client.json in the game's folder), else the old «uaRO»
std::string ourClient() {
    std::string text, error;
    client_patch::Manifest m;
    if (!readTextFile(layout::join(gameDir(), client_patch::kClientFile), text) || !client_patch::parse(text, m, error))
        return client_patch::kOldClient;
    return client_patch::clientName(m.client);
}

}  // namespace

void SeveralWindows::check(HWND window, const std::string& name, const Config& config) {
    if (!window) return;
    if (!ownKept_) {
        wchar_t own[256] = {};
        InternalGetWindowText(window, own, 256);  // no message to the game's window procedure
        own_ = titled_ = own;
        client_ = ourClient();
        ownKept_ = true;
    }
    const unsigned cp = gameCodePage(config);  // the game's text in the code page [roext] sets for it
    const bool on = config.windowTitleName;
    title(window, on ? titleOf(name, cp) : own_, name != named_ || on != titleOn_);
    named_ = name;
    titleOn_ = on;
    place(window, config.windowRememberPlace ? name : "");
}

std::wstring SeveralWindows::titleOf(const std::string& name, unsigned codePage) const {
    charstatus::Status status;
    const bool known = !name.empty() && readOwnStatus(status);
    const std::string map = name.empty() ? "" : readGameMap();
    const std::string text = window_title::title(utf8Of(widened(name, codePage)), known ? &status : nullptr, map,
                                                 client_, startup().lang);
    return widened(text, CP_UTF8);
}

void SeveralWindows::title(HWND window, const std::wstring& text, bool now) {
    const uint64_t ms = GetTickCount64();
    if (text == titled_ || (!now && ms - titledAt_ < kTitleMs)) return;  // a new character or the switch at once
    // past the game's own window procedure: Unicode whatever the window is, and nothing of the game's runs on it
    DefWindowProcW(window, WM_SETTEXT, 0, LPARAM(text.c_str()));
    titled_ = text;
    titledAt_ = ms;
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
