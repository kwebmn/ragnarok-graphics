#include "win/console_hint.hpp"

#include <windows.h>

#include <cstdint>
#include <string>

#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win::consolehint {
namespace {

constexpr uint32_t kWindowMgr = 0x727ED8, kPush = 0x4DB310;  // the chat's line (win/hotbar.cpp toggleBattle)
constexpr int kLineColour = 0x64FFFF;
constexpr ULONGLONG kAfterMs = 5000;
using PushFn = int(__attribute__((thiscall)) *)(uint32_t manager, int msg, const char* text, int colour, int more);

struct State {
    bool checked = false, done = false;
    ULONGLONG mapAt = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

void frame(unsigned codePage) {
    State& s = state();
    if (s.done) return;
    const std::string flag = gameDir() + "\\" + layout::kConsoleHint;
    if (!s.checked) {
        s.checked = true;
        if (GetFileAttributesA(flag.c_str()) != INVALID_FILE_ATTRIBUTES) {
            s.done = true;
            return;
        }
    }
    if (!inGameWorld() || readGameMap().empty()) {
        s.mapAt = 0;
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (!s.mapAt) s.mapAt = now;
    if (now - s.mapAt < kAfterMs) return;
    s.done = true;
    const std::string text = narrowed(widened(settings::tr(language(), settings::Text::ConsoleHintLine), CP_UTF8),
                                      codePage);
    reinterpret_cast<PushFn>(uintptr_t(kPush))(kWindowMgr, 1, text.c_str(), kLineColour, 0);
    writeFileAtomic(flag, std::string("shown\r\n"));
    LOG_INFO("console hint: «Alt+D» told in the chat once");
}

}  // namespace uagfx::win::consolehint
