#include "win/tk_mission.hpp"

#include <windows.h>

#include <string>

#include "core/settings/strings.hpp"
#include "core/tk_mission.hpp"
#include "win/code_page.hpp"
#include "win/log.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"

namespace uagfx::win::tkmissionwin {
namespace {

constexpr uint32_t kWindowMgr = 0x727ED8, kPush = 0x4DB310;  // the chat's line (win/console_hint.cpp)
constexpr int kLineColour = 0x64C8FF;                        // a light orange
constexpr ULONGLONG kWaitMs = 30000;                          // the reference's load waited for at most
using PushFn = int(__attribute__((thiscall)) *)(uint32_t manager, int msg, const char* text, int colour, int more);

struct State {
    int lastMob = 0;
    bool pending = false;
    tkmission::Mission mission;
    ULONGLONG at = 0;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

void packet(uint16_t id, const uint8_t* p) {
    if (id != tkmission::kPacketId) return;
    State& s = state();
    tkmission::Mission m;
    if (!tkmission::parse(p, tkmission::kPacketLen, m) || m.mob == s.lastMob) return;
    s.lastMob = m.mob;
    s.mission = m;
    s.pending = true;
    s.at = GetTickCount64();
}

void frame(unsigned codePage) {
    State& s = state();
    if (!s.pending) return;
    const std::shared_ptr<const refindex::Index> index = reference::index();
    if (!index) {
        if (GetTickCount64() - s.at > kWaitMs) {
            s.pending = false;
            LOG_INFO("taekwon: the mission's monster %d - the reference not loaded, nothing told", s.mission.mob);
        }
        return;
    }
    s.pending = false;
    tkmission::Mission m = s.mission;
    m.name = utf8Of(widened(m.name, codePage));
    const std::string line = tkmission::line(m, index.get(), language());
    if (line.empty()) return;
    const std::string text = narrowed(widened(line, CP_UTF8), codePage);
    reinterpret_cast<PushFn>(uintptr_t(kPush))(kWindowMgr, 1, text.c_str(), kLineColour, 0);
    if (s.told++ < 20) LOG_INFO("taekwon: the mission's monster %d told in the chat - %s", m.mob, line.c_str());
}

}  // namespace uagfx::win::tkmissionwin
