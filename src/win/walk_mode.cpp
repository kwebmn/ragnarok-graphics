#include "win/walk_mode.hpp"

#include "core/walk_mode.hpp"
#include "win/chat_window.hpp"
#include "win/game_camera.hpp"
#include "win/key_walk.hpp"
#include "win/log.hpp"
#include "win/setting_edit.hpp"

namespace uagfx::win::walkmodewin {
namespace {

struct State {
    walkmode::Mode mode;
    bool known = false;      // the settings' state taken once
    bool fileOn = false;     // [interface] walk_mode as last read
    bool skipChar = false;   // the toggle's letter (` ё ') not typed
    const Config* config = nullptr;
    std::string iniPath;
};

State& state() {
    static State s;
    return s;
}

void tellDirs(const State& s) { keywalk::setModeDirs(s.mode.on() ? s.mode.dirs() : 0); }

}  // namespace

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    if (m == WM_CHAR) {
        if (s.skipChar) {
            s.skipChar = false;
            return true;
        }
        // the mode walking and the chat's field in focus: no letter goes into it
        return s.mode.on() && !s.mode.typing() && inGameWorld() && chatwin::focusInChat();
    }
    if (m != WM_KEYDOWN && m != WM_KEYUP) return false;
    if (m == WM_KEYDOWN) s.skipChar = false;  // a layout whose ` gives no letter (a dead key): nothing else skipped
    if (!inGameWorld() || !chatwin::focusInChat()) return false;  // the login, a name's or a number's field: theirs
    walkmode::Key k;
    k.vk = unsigned(wp);
    k.scan = unsigned((lp >> 16) & 0xFF);
    k.extended = ((lp >> 24) & 1) != 0;
    k.down = m == WM_KEYDOWN;
    k.ctrl = GetKeyState(VK_CONTROL) < 0;
    k.alt = GetKeyState(VK_MENU) < 0;
    k.chatEmpty = chatwin::inputEmpty();
    const walkmode::Act act = s.mode.key(k);
    tellDirs(s);
    switch (act) {
    case walkmode::Act::Pass: return false;
    case walkmode::Act::Swallow: return true;
    case walkmode::Act::Toggle: {
        s.skipChar = true;
        LOG_INFO("keys: the walk mode %s (`)", s.mode.on() ? "on - W A S D walk, Enter opens the chat" : "off");
        if (s.config && !s.iniPath.empty())
            settingedit::set(s.iniPath, *s.config, "interface.walk_mode", s.mode.on() ? "1" : "0");
        s.fileOn = s.mode.on();
        return true;
    }
    // the game's Enter would send the empty line; in the game's battle mode it is its own typing switch (without it the
    // game wipes the line at each key)
    case walkmode::Act::OpenChat: return !chatwin::gameBattle();
    case walkmode::Act::Send: return false;     // the game's Enter sends it
    case walkmode::Act::CancelChat: chatwin::clearLine(); return true;
    }
    return false;
}

void lost() {
    State& s = state();
    s.mode.lost();
    tellDirs(s);
}

void frame(const Config& config, const std::string& iniPath) {
    State& s = state();
    s.config = &config;
    s.iniPath = iniPath;
    if (!s.known || config.walkMode != s.fileOn) {  // the start, or «Налаштування» switched it
        s.known = true;
        s.fileOn = config.walkMode;
        if (s.mode.on() != config.walkMode) {
            s.mode.set(config.walkMode);
            tellDirs(s);
            LOG_INFO("keys: the walk mode %s by the settings", config.walkMode ? "on" : "off");
        }
    }
}

bool on() { return state().mode.on(); }

bool typing() { return state().mode.typing(); }

}  // namespace uagfx::win::walkmodewin
