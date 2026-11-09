#include "win/old_windows.hpp"

#include <cstdint>

#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win::oldwindows {
namespace {

// uaRO.exe's window maker FUN_004d42d0: a UI window's number and where the manager (0x727ED8) keeps it - those no
// module of the driver reads (2026-11-09: mail 0x48/0x6E/0x77... and the auction never come on uaRO's server; told
// all the same)
struct Old {
    int id;
    uint32_t at;
};
constexpr Old kOld[] = {{0x07, 0x78},  {0x15, 0xE4},  {0x1A, 0xC4},  {0x1D, 0x108}, {0x2F, 0xE0},  {0x31, 0xE8},
                        {0x33, 0x1CC}, {0x34, 0x1D0}, {0x37, 0x15C}, {0x42, 0x188}, {0x48, 0x138}, {0x4C, 0x194},
                        {0x50, 0x1A0}, {0x51, 0x1A4}, {0x52, 0xCC},  {0x55, 0x1D4}, {0x5B, 0x1BC}, {0x67, 0x1B0},
                        {0x68, 0x124}, {0x6A, 0x1D8}, {0x6E, 0x130}, {0x77, 0x128}, {0x86, 0x210}, {0x87, 0x214}};
constexpr uint32_t kWindowMgr = 0x727ED8, kShown = 0x38, kList = kWindowMgr + 0x38;
bool g_told[sizeof kOld / sizeof kOld[0]] = {};
// 3.0.18-beta.4 (the tester's old «Please wait...» on accounts never entered here, place unknown): every window of the
// manager's list (its +0x38, the ones drawn) still shown - the new interface hides those it redraws -, once a class
constexpr size_t kMostListed = 512, kMostClasses = 64;
uint32_t g_classes[kMostClasses] = {};
size_t g_classCount = 0;

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

}  // namespace

bool toldClass(uint32_t cls) {
    for (size_t i = 0; i < g_classCount; ++i)
        if (g_classes[i] == cls) return true;
    if (g_classCount < kMostClasses) g_classes[g_classCount++] = cls;
    return false;
}

void frame() {
    const uint32_t head = read32(kList);
    size_t seen = 0;
    for (uint32_t node = head ? read32(head) : 0; node && node != head && seen < kMostListed; node = read32(node)) {
        ++seen;
        const uint32_t w = read32(node + 8);
        if (!w || !read32(w + kShown)) continue;
        const uint32_t cls = read32(w);
        if (cls && !toldClass(cls))
            LOG_INFO("old windows: a window of class 0x%X in the game's list shows (its place %d,%d, %dx%d)", cls,
                     int(read32(w + 0x14)), int(read32(w + 0x18)), int(read32(w + 0x1C)), int(read32(w + 0x20)));
    }
    for (size_t i = 0; i < sizeof kOld / sizeof kOld[0]; ++i) {
        if (g_told[i]) continue;
        const uint32_t w = read32(kWindowMgr + kOld[i].at);
        if (!w || !read32(w + kShown)) continue;
        g_told[i] = true;
        LOG_INFO("old windows: the game's UI window 0x%X (the manager's +0x%X, class 0x%X) shows - not redrawn yet",
                 unsigned(kOld[i].id), unsigned(kOld[i].at), read32(w));
    }
}

}  // namespace uagfx::win::oldwindows
