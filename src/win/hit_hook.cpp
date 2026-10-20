#include "win/hit_hook.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>

#include "core/game_camera.hpp"
#include "core/hit_flash.hpp"
#include "win/context.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

// The game's sound at a place: its file, the place from the player (x, y, z), its reach and a volume.
using PlayAtFn = void(__cdecl*)(const char* name, float x, float y, float z, int reach, int fade, uint32_t volume);
constexpr uint32_t kPlayAt = 0x4396B0;
const uint8_t kPlayAtCode[] = {0x55, 0x8B, 0xEC, 0xA1, 0x54, 0x7B, 0x6B, 0x00};  // push ebp; mov ebp,esp; mov eax,[]
// The player's place as the sounds' callers read it (0x54567A): the game mode, its world (+0C4h), the player (+3Ch),
// x y z from +4.
const uint8_t kPlayerCode[] = {0xB9, 0x78, 0xB5, 0x74, 0x00, 0xE8, 0x4C, 0x14, 0xFB, 0xFF, 0x8B, 0x80, 0xC4,
                               0x00, 0x00, 0x00, 0x8B, 0x4D, 0xC0, 0x8B, 0x40, 0x3C};
constexpr uint32_t kPlayerAt = 0x54567A, kModeKind = 0x74B5D0, kModeNow = 0x74B57C;
uintptr_t g_trampoline = 0;

void noteHit(const char* name, float x, float y, float z) {
    Context& c = Context::get();
    if (!c.effects().hitFlash || !c.enhanced()) return;
    if (*reinterpret_cast<const int*>(uintptr_t(kModeKind)) != 1) return;
    const uint32_t mode = *reinterpret_cast<const uint32_t*>(uintptr_t(kModeNow));
    const uint32_t world = mode ? *reinterpret_cast<const uint32_t*>(uintptr_t(mode + 0xC4)) : 0;
    const uint32_t player = world ? *reinterpret_cast<const uint32_t*>(uintptr_t(world + 0x3C)) : 0;
    if (!player) return;
    const float* at = reinterpret_cast<const float*>(uintptr_t(player + 4));
    float view[12];
    if (!readGameCamera(view)) return;
    const camera::Projection p = camera::projectionFor(c.screenWidth, c.screenHeight);
    float sx = 0, sy = 0;
    if (!hit_flash::toScreen(view, p, at[0] + x, at[1] + y, at[2] + z, sx, sy)) return;
    // 2.7.10: the place comes at the player's height - on a hill far from the monster's feet: given to the monster
    // drawn last frame under or over it (hit_flash::Monsters)
    const float heardX = sx, heardY = sy;
    const bool aimed = c.monsters.aim(sx, sy);
    c.flashes.add(sx, sy, c.nowMs());
    static int told = 0;  // the first hits in the log: heard, and whether a monster took them
    if (told < 8) {
        ++told;
        LOG_INFO("hit flash: %s heard at %.0f,%.0f - %s", name, heardX, heardY,
                 aimed ? "given to the monster there" : "no monster drawn there");
    }
}

void __cdecl playAt(const char* name, float x, float y, float z, int reach, int fade, uint32_t volume) {
    if (hit_flash::hitSound(name)) noteHit(name, x, y, z);
    reinterpret_cast<PlayAtFn>(g_trampoline)(name, x, y, z, reach, fade, volume);
}

}  // namespace

bool installHitHook() {
    static int done = -1;
    if (done >= 0) return done == 1;
    done = 0;
    auto* entry = reinterpret_cast<uint8_t*>(uintptr_t(kPlayAt));
    if (std::memcmp(entry, kPlayAtCode, sizeof kPlayAtCode) != 0 ||
        std::memcmp(reinterpret_cast<const void*>(uintptr_t(kPlayerAt)), kPlayerCode, sizeof kPlayerCode) != 0) {
        LOG_INFO("effects: no flash on a hit - the game's sounds are not where this build of uaRO.exe has them");
        return false;
    }
    // Its first three instructions (8 bytes, no jumps) in a trampoline, then on to the rest.
    auto* t = static_cast<uint8_t*>(VirtualAlloc(nullptr, 16, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!t) return false;
    std::memcpy(t, entry, 8);
    t[8] = 0xE9;
    const int32_t back = int32_t(uintptr_t(kPlayAt) + 8 - (reinterpret_cast<uintptr_t>(t) + 13));
    std::memcpy(t + 9, &back, 4);
    FlushInstructionCache(GetCurrentProcess(), t, 16);
    DWORD old = 0;
    if (!VirtualProtect(entry, 5, PAGE_EXECUTE_READWRITE, &old)) {
        VirtualFree(t, 0, MEM_RELEASE);
        return false;
    }
    g_trampoline = reinterpret_cast<uintptr_t>(t);
    uint8_t jump[5] = {0xE9, 0, 0, 0, 0};
    const int32_t to = int32_t(reinterpret_cast<uintptr_t>(&playAt) - (uintptr_t(kPlayAt) + 5));
    std::memcpy(jump + 1, &to, 4);
    std::memcpy(entry, jump, 5);
    DWORD unused = 0;
    VirtualProtect(entry, 5, old, &unused);
    FlushInstructionCache(GetCurrentProcess(), entry, 5);
    done = 1;
    LOG_INFO("effects: a monster hit flashes - the game's hits' sounds heard (uaRO.exe 0x%X)", kPlayAt);
    return true;
}

}  // namespace uagfx::win
