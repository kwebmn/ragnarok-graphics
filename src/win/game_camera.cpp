#include "win/game_camera.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/ai/texture_cache.hpp"
#include "core/char_name.hpp"
#include "core/game_camera.hpp"
#include "core/minimap_proto.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

// ReadProcessMemory on ourselves: an address that is not mapped fails instead of faulting.
bool read(uint32_t address, void* out, size_t size) {
    SIZE_T got = 0;
    return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(address)), out,
                                        size, &got) &&
           got == size;
}

// WriteProcessMemory likewise: a place that is not mapped (or not writable) fails
bool write(uint32_t address, const void* in, size_t size) {
    SIZE_T put = 0;
    return address && WriteProcessMemory(GetCurrentProcess(), reinterpret_cast<void*>(uintptr_t(address)), in, size,
                                         &put) &&
           put == size;
}

// The exe the addresses belong to: known once (it does not change while it runs).
bool knownBuild() {
    static int build = -1;
    if (build < 0) {
        float constants[5];
        build = read(camera::kConstants, constants, sizeof constants) && camera::knownConstants(constants) ? 1 : 0;
    }
    return build == 1;
}

// The game mode while it shows a world; 0 at the login and the character list, or in another build of the exe.
uint32_t gameMode() {
    uint32_t type = 0, mode = 0;
    if (!knownBuild() || !read(camera::kModeMgr + camera::kModeType, &type, 4) || type != 1 ||
        !read(camera::kModeMgr + camera::kModeCurrent, &mode, 4))
        return 0;
    return mode;
}

}  // namespace

bool readGameCamera(float view[12]) {
    uint32_t cview = 0;
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + camera::kModeView, &cview, 4) || !read(cview + camera::kViewMatrix, view, 12 * 4))
        return false;
    return camera::plausibleView(view);
}

bool readCameraTargetDepth(float& z) {
    uint32_t cview = 0;
    float view[12];
    float at[3];
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + camera::kModeView, &cview, 4) || !read(cview + camera::kViewMatrix, view, sizeof view) ||
        !read(cview + camera::kViewAt, at, sizeof at) || !camera::plausibleView(view))
        return false;
    z = camera::viewZ(view, at);
    return std::isfinite(z) && z > camera::kNear && z < camera::kFar;
}

bool knownGameBuild() { return knownBuild(); }

bool inGameWorld() { return gameMode() != 0; }

uint32_t readGameMode() { return gameMode(); }

bool readGame(uint32_t address, void* out, size_t size) { return read(address, out, size); }

bool readCameraPose(float& yaw, float& distance) {
    uint32_t cview = 0;
    const uint32_t mode = gameMode();
    float y = 0.0f, d = 0.0f;
    if (!mode || !read(mode + camera::kModeView, &cview, 4) || !cview || !read(cview + camera::kViewYawTo, &y, 4) ||
        !read(cview + camera::kViewDistanceTo, &d, 4) || !std::isfinite(y) || !std::isfinite(d) || d <= 0.0f)
        return false;
    yaw = y;
    distance = d;
    return true;
}

bool writeCameraPose(float yaw, float distance, bool exact) {
    uint32_t cview = 0;
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + camera::kModeView, &cview, 4) || !cview) return false;
    bool ok = write(cview + camera::kViewYawTo, &yaw, 4) && write(cview + camera::kViewDistanceTo, &distance, 4);
    if (exact)
        ok = write(cview + camera::kViewYaw, &yaw, 4) && write(cview + camera::kViewDistance, &distance, 4) && ok;
    return ok;
}

bool readCameraLimits(bool& outdoor, bool& ownLimits, int& limitFrom, int& limitSpan) {
    const uint32_t mode = gameMode();
    int32_t flag = 1, own = 0;
    int16_t from = 0, span = 0;
    if (!mode || !read(mode + camera::kModeOutdoor, &flag, 4) || !read(mode + camera::kModeOwnLimits, &own, 4) ||
        !read(mode + camera::kModeLimitFrom, &from, 2) || !read(mode + camera::kModeLimitSpan, &span, 2))
        return false;
    outdoor = flag != 0;
    ownLimits = own != 0;
    limitFrom = from;
    limitSpan = span;
    return true;
}

bool keepCameraFree(bool wanted) {
    static uint32_t heldMode = 0;  // the game mode whose building map's flag we hold, and that map
    static std::string heldMap;
    const uint32_t mode = gameMode();
    int32_t outdoor = 1;
    if (!mode || !read(mode + camera::kModeOutdoor, &outdoor, 4)) {
        heldMode = 0;
        return false;
    }
    auto* flag = reinterpret_cast<int32_t*>(uintptr_t(mode + camera::kModeOutdoor));
    if (wanted && outdoor == 0) {  // a building map just started (or the box ticked on it)
        *flag = 1;
        heldMode = mode;
        heldMap = readGameMap();
        LOG_INFO("camera: %s is a building - its camera free as outdoors ([camera] indoor)", heldMap.c_str());
        return true;
    }
    if (heldMode != mode || readGameMap() != heldMap) {  // another map: its flag is the game's again
        heldMode = 0;
        return false;
    }
    if (!wanted) {  // the box off on the building map: the game's own lock back
        *flag = 0;
        heldMode = 0;
        return false;
    }
    return true;
}

void limitZoom(bool wanted) {
    static float own = 0.0f;  // the game's limit as the driver found it
    float now = 0.0f;
    if (!knownBuild() || !read(camera::kZoomMost, &now, 4)) return;
    if (own == 0.0f) own = now >= 1000.0f && now <= 10000.0f ? now : 2040.0f;
    const float want = wanted ? std::min(camera::kZoomLimit, own) : own;
    if (now != want) {  // in the exe's .data: written as it is
        *reinterpret_cast<float*>(uintptr_t(camera::kZoomMost)) = want;
        LOG_INFO("camera: zoomed out %.0f at most (%s)", want, wanted ? "[camera] zoom_limit" : "the game's own");
    }
    uint32_t cview = 0;
    const uint32_t mode = gameMode();
    if (!wanted || !mode || !read(mode + camera::kModeView, &cview, 4) || !cview) return;
    for (uint32_t at : {camera::kViewDistanceTo, camera::kViewDistance}) {  // the game clamps at its next zoom alone
        float d = 0.0f;
        if (read(cview + at, &d, 4) && d > want) *reinterpret_cast<float*>(uintptr_t(cview + at)) = want;
    }
}

void raiseWaterBoxes(bool wanted) {
    struct Kept {
        uint32_t node;
        float centre, half;  // its box's height as the game had it
    };
    static std::vector<Kept> kept;  // the boxes raised in this world
    static std::unordered_set<uint32_t> keptAt;
    static uint32_t world = 0, firstChild = 0;
    static ULONGLONG checked = 0;
    uint32_t cview = 0, w = 0, ground = 0, child = 0;
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + camera::kModeView, &cview, 4) || !read(cview + camera::kViewWorld, &w, 4) || !w ||
        !read(w + camera::kWorldGround, &ground, 4) || !ground ||
        !read(w + camera::kWorldRoot + camera::kNodeChildren, &child, 4) || !child)
        return;
    if (w != world || child != firstChild) {  // another map's world: its tree is new
        kept.clear();
        keptAt.clear();
        world = w;
        firstChild = child;
        checked = 0;
    }
    if (!wanted) {
        for (const Kept& k : kept) {
            write(k.node + camera::kNodeCentre + 4, &k.centre, 4);
            write(k.node + camera::kNodeHalf + 4, &k.half, 4);
        }
        if (!kept.empty()) LOG_INFO("water: the game's own boxes of the map's %zu parts back", kept.size());
        kept.clear();
        keptAt.clear();
        checked = 0;
        return;
    }
    // once a map, then every ten seconds (a map without them in its .rsw makes them as its models come)
    const ULONGLONG now = GetTickCount64();
    if (checked && now - checked < 10000) return;
    checked = now;
    int32_t cells[2] = {0, 0};
    float zoom = 0.0f, level = 0.0f, waves = 0.0f;
    if (!read(ground + camera::kGroundWidth, cells, sizeof cells) || !read(ground + camera::kGroundZoom, &zoom, 4) ||
        !read(ground + camera::kGroundWaterLevel, &level, 4) || !read(ground + camera::kGroundWaveHeight, &waves, 4) ||
        cells[0] <= 0 || cells[0] > 4096 || cells[1] <= 0 || cells[1] > 4096 || !(zoom > 0.0f && zoom < 1000.0f) ||
        !(std::fabs(level) < 1e5f) || !(std::fabs(waves) < 1000.0f))
        return;  // no ground of this build's
    const float waterTop = level - std::fabs(waves);
    unsigned raised = 0, seen = 0;
    std::vector<std::pair<uint32_t, int>> next{{w + camera::kWorldRoot, 0}};  // a node and its level
    while (!next.empty() && seen < 2000) {
        const auto [node, depth] = next.back();
        next.pop_back();
        float centre = 0.0f, half = 0.0f, toCentre = 0.0f, toHalf = 0.0f;
        if (!read(node + camera::kNodeCentre + 4, &centre, 4) || !read(node + camera::kNodeHalf + 4, &half, 4))
            continue;
        ++seen;
        if (camera::raiseToWater(centre, half, waterTop, toCentre, toHalf) &&
            write(node + camera::kNodeCentre + 4, &toCentre, 4) && write(node + camera::kNodeHalf + 4, &toHalf, 4)) {
            ++raised;
            if (keptAt.insert(node).second) kept.push_back({node, centre, half});
        }
        uint32_t children[4] = {0, 0, 0, 0};
        if (depth + 1 < camera::kTreeLevels && read(node + camera::kNodeChildren, children, sizeof children))
            for (uint32_t c : children)
                if (c) next.push_back({c, depth + 1});
    }
    if (raised)
        LOG_INFO("water: %s - %u of the game's %u parts of the map raised to its water's top (%.1f): its own water up "
                 "to its far plane", readGameMap().c_str(), raised, seen, waterTop);
}

bool readGameCursor(int& x, int& y) {
    int32_t at[2] = {0, 0};
    if (!knownBuild() || !read(camera::kMouse + camera::kMouseX, &at[0], 4) ||
        !read(camera::kMouse + camera::kMouseY, &at[1], 4))
        return false;
    x = at[0];
    y = at[1];
    return true;
}

namespace {

// 3.0.11-beta.1: uaRO.exe's UI window manager (0x727ED8) and its window 14 (the minimap's zoom buttons; the game's
// mode draws the minimap while it exists, FUN_0055c220), its DeleteWindow / MakeWindow (thiscall, the window's number):
// the calls of the Ctrl+Tab handler at 0x46CB67 / 0x46CB7B
constexpr uint32_t kWindowMgr = 0x727ED8, kMinimapWindow = kWindowMgr + 0x7C, kModeMinimapAllowed = 0x280;
constexpr uint32_t kDeleteWindow = 0x4D1E60, kMakeWindow = 0x4D42D0;
constexpr int kMinimapId = 14;
const uint8_t kDeleteCode[] = {0x55, 0x8B, 0xEC, 0x51, 0x56, 0x8B, 0xF1, 0x57, 0x33, 0xFF, 0x39, 0x7E, 0x70};
const uint8_t kMakeCode[] = {0x55, 0x8B, 0xEC, 0x6A, 0xFF, 0x68, 0x36, 0xE2,
                             0x68, 0x00, 0x64, 0xA1, 0x00, 0x00, 0x00, 0x00};
using WindowFn = uint32_t(__attribute__((thiscall)) *)(void* manager, int id);
// The world's place of the player and of the map's cells (FUN_0055c220): mode+0xC4 the world; world+0x3C the player
// (x +4, z +0xC); world+0x40 the map's cells (width +0x110, height +0x114, a cell's size +0x118)
constexpr uint32_t kModeWorld = 0xC4, kWorldPlayer = 0x3C, kWorldCells = 0x40, kCellsSize = 0x110;

bool sameCode(uint32_t at, const uint8_t* code, size_t size) {
    uint8_t now[32];
    return size <= sizeof now && read(at, now, size) && std::memcmp(now, code, size) == 0;
}

bool knownWindowCalls() {
    static int known = -1;
    if (known < 0)
        known = knownBuild() && sameCode(kDeleteWindow, kDeleteCode, sizeof kDeleteCode) &&
                sameCode(kMakeWindow, kMakeCode, sizeof kMakeCode);
    return known == 1;
}

}  // namespace

bool readPlayerCell(int& x, int& y, int& width, int& height) {
    uint32_t world = 0, player = 0, cells = 0;
    float at[3];
    int size[3];
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + kModeWorld, &world, 4) || !read(world + kWorldPlayer, &player, 4) ||
        !read(world + kWorldCells, &cells, 4) || !read(player + 4, at, sizeof at) ||
        !read(cells + kCellsSize, size, sizeof size))
        return false;
    minimap::Cell c;
    if (!minimap::cellOf(at[0], at[2], size[0], size[1], size[2], c)) return false;
    x = c.x;
    y = c.y;
    width = size[0];
    height = size[1];
    return true;
}

bool readPlayerWorld(float at[3], int& width, int& height, int& cellSize) {
    uint32_t world = 0, player = 0, cells = 0;
    int size[3];
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + kModeWorld, &world, 4) || !read(world + kWorldPlayer, &player, 4) ||
        !read(world + kWorldCells, &cells, 4) || !read(player + 4, at, 3 * sizeof(float)) ||
        !read(cells + kCellsSize, size, sizeof size) || size[0] <= 0 || size[1] <= 0 || size[2] <= 0)
        return false;
    width = size[0];
    height = size[1];
    cellSize = size[2];
    return std::isfinite(at[0]) && std::isfinite(at[1]) && std::isfinite(at[2]);
}

bool gameMinimapShown() {
    uint32_t window = 0;
    return gameMode() && read(kMinimapWindow, &window, 4) && window != 0;
}

bool hideGameMinimap() {
    if (!knownWindowCalls() || !gameMinimapShown()) return false;
    reinterpret_cast<WindowFn>(uintptr_t(kDeleteWindow))(reinterpret_cast<void*>(uintptr_t(kWindowMgr)), kMinimapId);
    return !gameMinimapShown();
}

bool showGameMinimap() {
    uint32_t allowed = 0;
    const uint32_t mode = gameMode();
    if (!knownWindowCalls() || !mode || gameMinimapShown() || !read(mode + kModeMinimapAllowed, &allowed, 4) ||
        !allowed)
        return false;
    reinterpret_cast<WindowFn>(uintptr_t(kMakeWindow))(reinterpret_cast<void*>(uintptr_t(kWindowMgr)), kMinimapId);
    return gameMinimapShown();
}

std::string readGameMap() {
    char name[48] = {};
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + camera::kModeMap, name, sizeof name - 1)) return "";
    return ai::mapName(name, sizeof name - 1);
}

std::string readOwnName() {
    static int known = -1;  // GetCharName as this build has it: the name's place and its key are where they are read
    if (known < 0) {
        uint8_t code[charname::kCodeSize];
        known = knownBuild() && read(charname::kGetCharName, code, sizeof code) &&
                std::memcmp(code, charname::kCode, sizeof code) == 0;
    }
    uint8_t stored[charname::kStored], key[charname::kStored];
    uint32_t keyAt = 0;
    if (known != 1 || !gameMode() || !read(charname::kSession + charname::kNameAt, stored, sizeof stored) ||
        !read(charname::kKeyAt, &keyAt, 4) || !read(keyAt, key, sizeof key))
        return "";
    return charname::decode(stored, key);
}

bool readOwnStatus(charstatus::Status& s) {
    static int known = -1;  // the handlers and the unmasking as this build has them: the numbers are where read
    if (known < 0) {
        known = knownBuild() ? 1 : 0;
        for (size_t i = 0; known == 1 && i < charstatus::kProbeCount; ++i) {
            const charstatus::Probe& p = charstatus::kProbes[i];
            uint8_t code[sizeof p.bytes];
            known = read(p.at, code, p.size) && std::memcmp(code, p.bytes, p.size) == 0;
        }
        uint8_t code[charstatus::kUnmaskSize];
        known = known == 1 && read(charstatus::kUnmaskAt, code, sizeof code) &&
                charstatus::fnv1a(code, sizeof code) == charstatus::kUnmaskFnv;
        LOG_INFO("windows: the character's levels, experience, HP and SP %s",
                 known ? "are read for the window's title"
                       : "are not where this build of uaRO.exe keeps them - the title shows the name and the map");
    }
    uint8_t plain[charstatus::kPlainSize], masked[charstatus::kMaskedSize];
    if (known != 1 || !gameMode() || !read(charstatus::kPlainAt, plain, sizeof plain) ||
        !read(charstatus::kMaskedAt, masked, sizeof masked))
        return false;
    s = charstatus::fromMemory(plain, masked);
    return true;
}

}  // namespace uagfx::win
