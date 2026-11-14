#include "win/ground_thin.hpp"

#include <windows.h>

#include <cstdint>

#include "core/ground_thin.hpp"
#include "win/context.hpp"
#include "win/entry_hook.hpp"
#include "win/log.hpp"
#include "win/view3d.hpp"

namespace uagfx::win::groundthin {
namespace {

// uaRO.exe's ground cell (FUN_004E5E20, thiscall on the ground: the view matrix, the cell's face - four corners 0x20
// bytes apart, each its place first -, a flag; ret 0xC; FUN_004E5D?? calls it for a cell's top and its two walls), its
// first 9 bytes (push ebp; mov ebp, esp; sub esp, 0xBC)
constexpr uint32_t kCell = 0x4E5E20;
constexpr uint8_t kCellBytes[9] = {0x55, 0x8B, 0xEC, 0x81, 0xEC, 0xBC, 0x00, 0x00, 0x00};
using CellFn = void(__attribute__((thiscall)) *)(uint32_t ground, uint32_t matrix, uint32_t cell, int flag);
uintptr_t g_cell = 0;
bool g_on = false;
// the renderer's word, once a camera build
uint32_t g_askedAt = 0xFFFFFFFFu;
bool g_lately = false;
uint64_t g_worked = 0, g_left = 0;
DWORD g_toldAt = 0;
bool g_firstTold = false;

void __attribute__((thiscall)) cellHook(uint32_t ground, uint32_t matrix, uint32_t cell, int flag) {
    if (g_on && cell && view3dwin::active()) {
        const uint32_t build = view3dwin::builds();
        if (build != g_askedAt) {
            g_askedAt = build;
            const gpu::Renderer* r = Context::get().renderer;
            g_lately = r && r->ownGroundLately();
        }
        float depth = 0.0f;
        // the game reads the same corners right after: they are there
        const float* at = reinterpret_cast<const float*>(uintptr_t(cell));
        if (g_lately && view3dwin::viewDepthOf(at, depth) && !ground_thin::workedOut(depth, cell, build)) {
            ++g_left;
            return;
        }
    }
    ++g_worked;
    reinterpret_cast<CellFn>(g_cell)(ground, matrix, cell, flag);
}

}  // namespace

bool install() {
    if (g_cell) return true;
    if (!hookEntryBytes(kCell, kCellBytes, sizeof kCellBytes, reinterpret_cast<const void*>(&cellHook), g_cell)) {
        LOG_INFO("3D view: the game's ground cell is not where this build of uaRO.exe has it - every one worked out");
        return false;
    }
    LOG_INFO("3D view: the game's ground cells hooked (uaRO.exe 0x%X) - the far ones fewer while the own ground is "
             "drawn in their place", kCell);
    return true;
}

void setOn(bool on) { g_on = on; }

void frame() {
    const DWORD now = GetTickCount();
    if (!g_left || (g_firstTold && now - g_toldAt < 30000)) return;
    const uint64_t all = g_worked + g_left;
    LOG_INFO("3D view: the game's ground cells - %llu of %llu worked out (%.0f %%), the far ones past %.0f one frame "
             "in %u: the renderer's own ground drawn in their place", (unsigned long long)g_worked,
             (unsigned long long)all, all ? 100.0 * double(g_worked) / double(all) : 100.0,
             double(ground_thin::kNearDepth), ground_thin::kEvery);
    g_firstTold = true;
    g_toldAt = now;
    g_worked = g_left = 0;
}

}  // namespace uagfx::win::groundthin
