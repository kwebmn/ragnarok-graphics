#include "win/pick_hook.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "core/ground_pick.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

// uaRO.exe's: the ground pick (the game mode in ECX, the cell's x and y written when one is found; a bool in AL),
// the mouse's ray (the camera in ECX: the mouse, the eye, the view's matrix, the ray written) and one cell's test
// (the ground in ECX: the distance along the ray in ST0, 999999 for none).
using PickFn = unsigned char(__attribute__((thiscall)) *)(uint32_t mode, int* x, int* y);
using RayFn = void(__attribute__((thiscall)) *)(uint32_t camera, int mx, int my, uint32_t eye, uint32_t matrix,
                                                float* ray);
using CellFn = float(__attribute__((thiscall)) *)(uint32_t ground, float* ray, int x, int y);
constexpr uint32_t kPick = 0x564A20, kRay = 0x410CA0, kCell = 0x42A9A0;
constexpr uint32_t kCall = 0x56650F;                    // the pick's one call (FUN_005664d0)
constexpr uint32_t kCamera = 0x6EE3D8, kMouseX = 0x6EE704, kMouseY = 0x6EE708;
constexpr uint32_t kWorld = 0xC4, kView = 0xC8;         // the game mode's
constexpr uint32_t kTree = 0x68, kLeafGround = 0x6C;    // the world's map tree (5 levels), a leaf's ground
constexpr uint32_t kWidth = 0x110, kHeight = 0x114, kSize = 0x118, kCells = 0x120;  // the ground's

// The bytes the pick and its call are made of in the build this knows: its own reads of the tree and the ground.
struct Known {
    uint32_t at;
    const char* bytes;
    size_t size;
};
const Known kKnown[] = {
    {0x56650D, "\x8B\xCB\xE8\x0C\xE5\xFF\xFF\x84\xC0", 9},           // mov ecx,ebx; call pick; test al,al
    {0x564A48, "\x8B\x86\xC8\x00\x00\x00", 6},                       // the view
    {0x564A71, "\x8B\x0D\xD8\xE3\x6E\x00\xE8\x24\xC2\xEA\xFF", 11},  // the camera; call the ray
    {0x564A7C, "\x8B\x96\xC4\x00\x00\x00\x8D\x45\xD0\x83\xC2\x68", 12},
    {0x564AAD, "\x8B\x7B\x74\x8B\x43\x7C\x8B\x53\x6C", 9},           // a leaf's cells and ground
    {0x564ABD, "\x8B\x73\x70\x8B\x43\x78", 6},
    {0x564AD0, "\xE8\xCB\x5E\xEC\xFF", 5},                           // call the cell's test
    {0x42AAF7, "\xC2\x0C\x00", 3},
    {0x4ED4CE, "\x8D\x79\x04\x8B\x17", 5},                       // the tree's children: a node's four from +4
    {0x4ED5B8, "\x83\xF8\x05", 3},                                   // its leaves five levels down
};

pick::Trust g_trust;
bool g_on = false;
LARGE_INTEGER g_freq{};
int64_t g_theirs = 0, g_ours = 0;  // the ticks both took while checked on the map
// 3.0.2-beta.13: the picks, those checked and the game's own ticks on them - all of the game's (pickCounts)
unsigned g_picks = 0, g_checked = 0;
int64_t g_theirsAll = 0;

uint32_t at(uint32_t address) { return *reinterpret_cast<volatile const uint32_t*>(uintptr_t(address)); }

int64_t ticks() {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

// The map's ground, as the tree's leaves hold it (the first one's): 0 while there is none.
uint32_t groundOf(uint32_t world) {
    uint32_t node = world + kTree;
    for (int level = 0; level < 5 && node; ++level) node = at(node + 4);
    return node ? at(node + kLeafGround) : 0;
}

unsigned char __attribute__((thiscall)) pickCell(uint32_t mode, int* x, int* y) {
    const auto original = reinterpret_cast<PickFn>(uintptr_t(kPick));
    ++g_picks;
    if (!g_on || g_trust.off()) return original(mode, x, y);
    const uint32_t world = at(mode + kWorld), view = at(mode + kView), camera = at(kCamera);
    const uint32_t ground = world && view && camera ? groundOf(world) : 0;
    if (!ground) return original(mode, x, y);
    const int w = int(at(ground + kWidth)), h = int(at(ground + kHeight)), size = int(at(ground + kSize));
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || size <= 0 || !at(ground + kCells)) return original(mode, x, y);
    if (g_trust.on(ground ^ (at(ground + kCells) * 31u) ^ (uint32_t(w) << 20) ^ uint32_t(h)))  // another map
        g_ours = g_theirs = 0;
    if (g_trust.settling()) return 0;  // 3.0.2-beta.13: the game's own answer is none then, at 7.6 ms a time
    const bool checking = g_trust.check();
    const int64_t start = checking ? ticks() : 0;
    float ray[6] = {};
    reinterpret_cast<RayFn>(uintptr_t(kRay))(camera, int(at(kMouseX)), int(at(kMouseY)), view + 0x7C, view + 0xC4,
                                             ray);
    const auto cell = reinterpret_cast<CellFn>(uintptr_t(kCell));
    int cx = 0, cy = 0;
    float distance = 0;
    const bool hit = pick::walk(ray, w, h, float(size), [&](int i, int j) { return cell(ground, ray, i, j); }, cx, cy,
                                distance, 0.0f);  // 0.90.1: in front of the eye, as the game's pieces of the map
    if (!checking) {
        if (hit) {
            *x = cx;
            *y = cy;
        }
        return hit ? 1 : 0;
    }
    // Checked: the game's own answer is the one used.
    const int64_t middle = ticks();
    int gx = *x, gy = *y;
    const bool theirs = original(mode, &gx, &gy) != 0;
    const int64_t ended = ticks();
    ++g_checked;
    g_theirsAll += ended - middle;
    bool same = theirs == hit;
    if (same && hit && (gx != cx || gy != cy))  // two cells at the same distance: the ray at their shared edge
        same = pick::sameDistance(cell(ground, ray, gx, gy), distance);
    if (!g_trust.trusted()) {  // the map's first checks: both timed
        g_ours += middle - start;
        g_theirs += ended - middle;
    }
    if (g_trust.result(same)) {
        if (!g_freq.QuadPart) QueryPerformanceFrequency(&g_freq);
        const double n = pick::Trust::kAgree * double(g_freq.QuadPart) / 1000.0;
        LOG_INFO("pick: the cell under the mouse found the fast way on this map (%dx%d cells) - %.3f ms a time, the "
                 "game's own %.3f ms", w, h, double(g_ours) / n, double(g_theirs) / n);
        g_ours = g_theirs = 0;
    } else if (!same) {
        const float their = theirs ? cell(ground, ray, gx, gy) : pick::kNone;
        LOG_INFO("pick: %s - the fast one was %s (%d, %d) at %.1f, the game's %s (%d, %d) at %.1f on a map of %dx%d "
                 "cells", g_trust.off() ? "the game's own cell under the mouse from now on" : "checked anew",
                 hit ? "at" : "none", cx, cy, double(hit ? distance : 0.0f), theirs ? "at" : "none", gx, gy,
                 double(theirs ? their : 0.0f), w, h);
    }
    if (theirs) {
        *x = gx;
        *y = gy;
    }
    return theirs ? 1 : 0;
}

}  // namespace

void pickCounts(unsigned& picks, unsigned& checked, double& theirsMs) {
    if (!g_freq.QuadPart) QueryPerformanceFrequency(&g_freq);
    picks = g_picks;
    checked = g_checked;
    theirsMs = double(g_theirsAll) * 1000.0 / double(g_freq.QuadPart);
}

bool installPickHook(bool on) {
    static int done = -1;  // once: 1 hooked, 0 not
    if (done >= 0) return done == 1;
    done = 0;
    if (!on) {
        LOG_INFO("pick: the game's own cell under the mouse ([debug] fast_pick = 0)");
        return false;
    }
    for (const Known& k : kKnown) {
        uint8_t bytes[16] = {};
        SIZE_T got = 0;
        if (!ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(k.at)), bytes, k.size,
                               &got) ||
            got != k.size || std::memcmp(bytes, k.bytes, k.size) != 0) {
            LOG_INFO("pick: the game's own cell under the mouse - its code is not where this build of uaRO.exe has it "
                     "(0x%X)", k.at);
            return false;
        }
    }
    auto* rel = reinterpret_cast<uint8_t*>(uintptr_t(kCall + 1));
    DWORD old = 0;
    if (!VirtualProtect(rel, 4, PAGE_EXECUTE_READWRITE, &old)) {
        LOG_INFO("pick: the game's own cell under the mouse - its code cannot be changed (error %lu)", GetLastError());
        return false;
    }
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(&pickCell) - (uintptr_t(kCall) + 5));
    std::memcpy(rel, &to, sizeof to);
    DWORD unused = 0;
    VirtualProtect(rel, 4, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), rel, 4);
    g_on = true;
    done = 1;
    LOG_INFO("pick: the cell under the mouse found along its ray (uaRO.exe 0x%X), checked against the game's own on "
             "each map first", kPick);
    return true;
}

}  // namespace uagfx::win
