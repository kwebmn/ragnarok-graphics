#include "win/login_refusal.hpp"

#include <windows.h>

#include <cstdint>

#include "win/code_detour.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win::loginrefusal {
namespace {

// uaRO.exe's: CLoginMode's refusal handler and its first instructions (push ebp; mov ebp, esp; sub esp, 0x100)
constexpr uint32_t kRefused = 0x5A40C0;
const uint8_t kRefusedCode[] = {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0x00, 0x01, 0x00, 0x00};
using RefusedFn = void(__attribute__((thiscall)) *)(uint32_t mode, const uint8_t* packet);

uintptr_t g_refused = 0;
int g_handling = -1;
std::string g_until;
std::optional<Seen> g_last;
int g_told = 0;

void __attribute__((thiscall)) refused(uint32_t mode, const uint8_t* packet) {
    uint8_t head[3 + 20] = {};
    const bool read = packet && readGame(uint32_t(reinterpret_cast<uintptr_t>(packet)), head, sizeof head);
    g_handling = read ? head[2] : -1;
    g_until.clear();
    if (read && (head[2] == 6 || head[2] == 100)) {
        char date[21] = {};
        for (int i = 0; i < 20; ++i) date[i] = char(head[3 + i]);
        g_until = date;
    }
    if (g_told++ < 20) LOG_INFO("login: the server refused the login (code %d)", g_handling);
    reinterpret_cast<RefusedFn>(g_refused)(mode, packet);
    if (g_handling >= 0) g_last = Seen{g_handling, g_until};
    g_handling = -1;
}

}  // namespace

void install() {
    static bool done = false;
    if (done) return;
    done = true;
    const void* ours = reinterpret_cast<const void*>(&refused);
    g_refused = codedetour::detour(kRefused, kRefusedCode, sizeof kRefusedCode, ours);
    LOG_INFO("login: the server's refusals %s", g_refused ? "caught" : "not caught (another build of uaRO.exe)");
}

int handlingCode() { return g_handling; }

const std::string& handlingUntil() { return g_until; }

std::optional<Seen> take() {
    std::optional<Seen> out = g_last;
    g_last.reset();
    return out;
}

}  // namespace uagfx::win::loginrefusal
