#include "win/detail_window.hpp"

#include <windows.h>

#include <cstdint>

#include "win/console.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/log.hpp"

namespace uagfx::win::detailwin {
namespace {

// uaRO.exe's: the detail window (where the manager keeps it, its class's vtable; its shown flag) and DeleteWindow
constexpr uint32_t kWindowMgr = 0x727ED8, kDetailAt = kWindowMgr + 0x168, kDetailClass = 0x698798, kShown = 0x38;
constexpr uint32_t kDeleteWindow = 0x4D1E60;
constexpr int kDetailWindow = 0x39;
using DeleteFn = char(__attribute__((thiscall)) *)(uint32_t manager, int id);

bool g_on = false;
int g_told = 0;

}  // namespace

void frame(bool on) {
    if (on != g_on) LOG_INFO("detail: [interface] detail_console = %d", on ? 1 : 0);
    g_on = on;
}

void beforeGameDraw() {
    if (!g_on) return;
    const uint32_t w = read32(kDetailAt);
    if (!w || read32(w) != kDetailClass) return;
    uint32_t off = 0;
    SIZE_T n = 0;
    WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(w + kShown)), &off, 4, &n);
    reinterpret_cast<DeleteFn>(uintptr_t(kDeleteWindow))(kWindowMgr, kDetailWindow);
    Console::askOpen();
    if (g_told++ < 20) LOG_INFO("detail: the game's detail window (Alt+D) closed - the driver's console instead");
}

}  // namespace uagfx::win::detailwin
