#include "win/game_camera.hpp"

#include "pad/win/runtime.hpp"

#include <windows.h>
#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/ai/texture_cache.hpp"
#include "core/char_name.hpp"
#include "core/game_camera.hpp"
#include "core/camera_memory.hpp"
#include "core/layout.hpp"
#include "core/minimap_proto.hpp"
#include "core/read_tally.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/safe_read.hpp"

namespace uagfx::win {
namespace {

// 3.0.18-beta.161 (the owner: «делай 1 и 2»): the reads on the game's thread counted (core/read_tally) - each one a
// call into Windows - by the place outside this file that asked: the outermost of its functions (Asked) while it runs
readtally::Tally g_tally;
DWORD g_tallyThread = 0;
uint32_t g_asker = 0;
double g_msPerTick = 0;

struct Asked {
    bool outer;
    explicit Asked(void* from) : outer(g_asker == 0 && g_tallyThread && GetCurrentThreadId() == g_tallyThread) {
        if (outer) g_asker = uint32_t(uintptr_t(from));
    }
    ~Asked() {
        if (outer) g_asker = 0;
    }
};

// An address that is not mapped fails instead of faulting: the processor's own copy with our fault handler
// (win/safe_read, 3.0.18-beta.165), else ReadProcessMemory on ourselves as before.
bool copyOut(uint32_t address, void* out, size_t size) {
    if (saferead::ready()) return saferead::copy(out, address, size);
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(address)), out, size, &got) &&
           got == size;
}

bool read(uint32_t address, void* out, size_t size) {
    if (!address) return false;
    if (!g_tallyThread || GetCurrentThreadId() != g_tallyThread) return copyOut(address, out, size);
    LARGE_INTEGER a, b;
    QueryPerformanceCounter(&a);
    const bool ok = copyOut(address, out, size);
    QueryPerformanceCounter(&b);
    g_tally.add(g_asker ? g_asker : uint32_t(uintptr_t(__builtin_return_address(0))),
                double(b.QuadPart - a.QuadPart) * g_msPerTick);
    return ok;
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

// 3.0.11-beta.184 (R-ANMTEJ): the map whose start win/map_start saw end - its game mode, the game's own outdoor flag
// and own camera (data\viewpointtable.txt) for it -, the building map whose flag the driver holds at 1, what the frames
// last asked ([camera] indoor, remember), the slots kept over a map of its own camera, what camera.txt holds
uint32_t g_startedMode = 0;
bool g_startedOutdoor = true, g_startedOwn = false;
uint32_t g_heldMode = 0;
bool g_freeWanted = true, g_rememberWanted = false, g_memoryApplied = false;
std::optional<cameramemory::Memory> g_keptSlots, g_written;
int g_toldExits = 0, g_toldWrites = 0;

std::string memoryFile() { return layout::join(gameDir(), layout::kCameraMemory); }

std::string readMap(uint32_t mode) {
    char name[40] = {};
    if (!read(mode + camera::kModeMap, name, sizeof name - 1)) return {};
    std::string map(name);
    const size_t dot = map.find('.');
    return dot == std::string::npos ? map : map.substr(0, dot);
}

cameramemory::Memory slotsNow() {
    cameramemory::Memory m;
    read(camera::kOutdoorTilt, &m.outdoors.tilt, 4);
    read(camera::kOutdoorZoom, &m.outdoors.zoom, 4);
    read(camera::kBuildingTilt, &m.buildings.tilt, 4);
    read(camera::kBuildingZoom, &m.buildings.zoom, 4);
    return m;
}

void putSlots(const cameramemory::Memory& m) {
    write(camera::kOutdoorTilt, &m.outdoors.tilt, 4);
    write(camera::kOutdoorZoom, &m.outdoors.zoom, 4);
    write(camera::kBuildingTilt, &m.buildings.tilt, 4);
    write(camera::kBuildingZoom, &m.buildings.zoom, 4);
}

// The file written when the camera moved on from what it holds.
void remember(const cameramemory::Memory& m) {
    if (g_written && !cameramemory::differs(m, *g_written)) return;
    if (cameramemory::parse(cameramemory::format(m)) && writeFileAtomic(memoryFile(), cameramemory::format(m))) {
        g_written = m;
        if (g_toldWrites++ < 5)
            LOG_INFO("camera: remembered - outdoors %.0f (tilt %.0f), in buildings %.0f (tilt %.0f)", m.outdoors.zoom,
                     m.outdoors.tilt, m.buildings.zoom, m.buildings.tilt);
    }
}

// A building map's camera as free as outdoors: the outdoor flag 1, and the zoom and tilt the buildings' slot keeps -
// the game's start put the zoom at 300 at most, its buildings' own limit
void freeBuilding(uint32_t mode, const char* why) {
    *reinterpret_cast<int32_t*>(uintptr_t(mode + camera::kModeOutdoor)) = 1;
    g_heldMode = mode;
    float tilt = 0.0f, zoom = 0.0f, least = 0.0f, most = 0.0f, now = 0.0f;
    uint32_t cview = 0;
    if (!g_startedOwn && read(camera::kBuildingTilt, &tilt, 4) && read(camera::kBuildingZoom, &zoom, 4) &&
        read(camera::kZoomLeast, &least, 4) && read(camera::kZoomMost, &most, 4) && read(camera::kZoomNow, &now, 4) &&
        read(mode + camera::kModeView, &cview, 4) && cview && std::isfinite(zoom) && least > 0.0f && most > least &&
        zoom > now + 0.5f) {
        zoom = std::clamp(zoom, least, most);
        tilt = std::clamp(std::isfinite(tilt) ? tilt : -45.0f, -65.0f, -25.0f);  // as an outdoors start: -45 +-20
        write(camera::kZoomNow, &zoom, 4);
        write(camera::kTiltNow, &tilt, 4);
        for (uint32_t at : {camera::kViewDistance, camera::kViewDistanceTo}) write(cview + at, &zoom, 4);
        for (uint32_t at : {camera::kViewTilt, camera::kViewTiltTo}) write(cview + at, &tilt, 4);
    }
    float shown = 0.0f;
    read(camera::kZoomNow, &shown, 4);
    LOG_INFO("camera: %s is a building - its camera free as outdoors ([camera] indoor, %s; zoom %.0f)",
             readMap(mode).c_str(), why, shown);
}

}  // namespace

bool readGameCamera(float view[12]) {
    const Asked asked(__builtin_return_address(0));
    uint32_t cview = 0;
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + camera::kModeView, &cview, 4) || !read(cview + camera::kViewMatrix, view, 12 * 4))
        return false;
    return camera::plausibleView(view);
}

bool readCameraTargetDepth(float& z) {
    const Asked asked(__builtin_return_address(0));
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

bool readCameraAndTarget(float view[12], float at[3]) {
    const Asked asked(__builtin_return_address(0));
    uint32_t cview = 0;
    const uint32_t mode = gameMode();
    return mode && read(mode + camera::kModeView, &cview, 4) && read(cview + camera::kViewMatrix, view, 12 * 4) &&
           read(cview + camera::kViewAt, at, 12) && camera::plausibleView(view);
}

bool knownGameBuild() { return knownBuild(); }

bool inGameWorld() {
    const Asked asked(__builtin_return_address(0));
    return gameMode() != 0;
}

uint32_t readGameMode() {
    const Asked asked(__builtin_return_address(0));
    return gameMode();
}

bool readGame(uint32_t address, void* out, size_t size) {
    const Asked asked(__builtin_return_address(0));
    return read(address, out, size);
}

bool readCameraPose(float& yaw, float& distance) {
    const Asked asked(__builtin_return_address(0));
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
    const Asked asked(__builtin_return_address(0));
    uint32_t cview = 0;
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + camera::kModeView, &cview, 4) || !cview) return false;
    bool ok = write(cview + camera::kViewYawTo, &yaw, 4) && write(cview + camera::kViewDistanceTo, &distance, 4);
    if (exact)
        ok = write(cview + camera::kViewYaw, &yaw, 4) && write(cview + camera::kViewDistance, &distance, 4) && ok;
    return ok;
}

bool readCameraLimits(bool& outdoor, bool& ownLimits, int& limitFrom, int& limitSpan) {
    const Asked asked(__builtin_return_address(0));
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
    const Asked asked(__builtin_return_address(0));
    g_freeWanted = wanted;
    const uint32_t mode = gameMode();
    if (!mode || mode != g_startedMode || g_startedOutdoor) return false;  // not a building whose start was seen
    if (wanted && g_heldMode != mode) {  // the box ticked on a building map
        freeBuilding(mode, "the box ticked");
        return true;
    }
    if (!wanted && g_heldMode == mode) {  // the box off on it: the game's own lock back
        *reinterpret_cast<int32_t*>(uintptr_t(mode + camera::kModeOutdoor)) = 0;
        g_heldMode = 0;
        return false;
    }
    return g_heldMode == mode;
}

void cameraBeforeMapStart() {
    g_startedMode = 0;  // a new map: nothing of it known until the game's start is over (the same address, often)
    if (g_memoryApplied || !knownBuild()) return;
    g_memoryApplied = true;  // the first map of the game only: the slots are the game's own from then on
    if (!g_rememberWanted) return;
    std::string text;
    if (!readTextFile(memoryFile(), text)) return;
    const std::optional<cameramemory::Memory> m = cameramemory::parse(text);
    if (!m) {
        LOG_INFO("camera: %s not understood - the game's own camera", layout::kCameraMemory);
        return;
    }
    putSlots(*m);
    g_written = m;
    LOG_INFO("camera: as left last time - outdoors %.0f (tilt %.0f), in buildings %.0f (tilt %.0f) ([camera] remember)",
             m->outdoors.zoom, m->outdoors.tilt, m->buildings.zoom, m->buildings.tilt);
}

void cameraAfterMapStart(uint32_t mode) {
    int32_t outdoor = 1, own = 0;
    if (!mode || !knownBuild() || !read(mode + camera::kModeOutdoor, &outdoor, 4) ||
        !read(mode + camera::kModeOwnLimits, &own, 4))
        return;
    g_startedMode = mode;
    g_startedOutdoor = outdoor != 0;
    g_startedOwn = own != 0;
    g_heldMode = 0;
    if (g_freeWanted && !g_startedOutdoor) freeBuilding(mode, "its start");
}

void cameraBeforeMapExit(uint32_t mode) {
    g_keptSlots.reset();
    if (!mode || mode != g_startedMode) return;
    if (g_startedOwn) g_keptSlots = slotsNow();  // its own camera (data\viewpointtable.txt): not the player's choice
    if (mode != g_heldMode) return;
    *reinterpret_cast<int32_t*>(uintptr_t(mode + camera::kModeOutdoor)) = 0;
    g_heldMode = 0;
    float zoom = 0.0f, outdoors = 0.0f;
    if (g_toldExits++ < 20 && read(camera::kZoomNow, &zoom, 4) && read(camera::kOutdoorZoom, &outdoors, 4))
        LOG_INFO("camera: leaving the building %s - its zoom %.0f kept with the buildings', the outdoors' %.0f stays",
                 readMap(mode).c_str(), zoom, outdoors);
}

void cameraAfterMapExit(uint32_t mode) {
    if (g_keptSlots) {
        putSlots(*g_keptSlots);
        g_keptSlots.reset();
    }
    if (mode && mode == g_startedMode) g_startedMode = 0;
    if (g_rememberWanted && knownBuild()) remember(slotsNow());
}

namespace {
float g_tiltRoom = 70.0f;  // what the game's two tilt limits read once pointed here
}

void tiltCamera(const std::string& token) {
    static int pointed = -1;  // -1 not tried, 0 not this build, 1 pointed
    const float want = camera::tiltRoom(token);
    if (pointed < 0) {
        pointed = 0;
        const uint8_t game[6] = {0xD8, 0x25, 0xF0, 0xF3, 0x69, 0x00};
        bool known = true;
        for (const uint32_t site : camera::kTiltRoomSites) {
            uint8_t now[6] = {};
            known = known && read(site, now, sizeof now) && std::memcmp(now, game, sizeof now) == 0;
        }
        float seventy = 0.0f;
        known = known && read(camera::kTiltRoomGame, &seventy, 4) && seventy == 70.0f;
        if (!known) {
            LOG_INFO("camera: the tilt's limit is not where this build of uaRO.exe has it - [camera] tilt the game's");
            return;
        }
        const uint32_t ours = uint32_t(reinterpret_cast<uintptr_t>(&g_tiltRoom));
        for (const uint32_t site : camera::kTiltRoomSites) {
            auto* at = reinterpret_cast<uint8_t*>(uintptr_t(site + 2));  // the instruction's address operand
            DWORD old = 0;
            if (!VirtualProtect(at, 4, PAGE_EXECUTE_READWRITE, &old)) {
                LOG_INFO("camera: the tilt's limit cannot be changed (error %lu)", GetLastError());
                return;
            }
            std::memcpy(at, &ours, 4);
            DWORD unused = 0;
            VirtualProtect(at, 4, old, &unused);
            FlushInstructionCache(GetCurrentProcess(), at, 4);
        }
        pointed = 1;
    }
    if (pointed != 1 || g_tiltRoom == want) return;
    g_tiltRoom = want;
    LOG_INFO("camera: [camera] tilt = %s - Shift tilts it to %.0f degrees outdoors", token.c_str(), 42.0f - want);
}

void rememberCamera(bool wanted) {
    g_rememberWanted = wanted;
    if (!wanted || !knownBuild()) return;
    static DWORD last = 0;
    const DWORD now = GetTickCount();
    if (now - last < 2000) return;
    last = now;
    const uint32_t mode = gameMode();
    if (!mode || mode != g_startedMode || g_startedOwn) return;  // a world whose start was seen, the player's camera
    cameramemory::Pose pose;
    if (!read(camera::kTiltNow, &pose.tilt, 4) || !read(camera::kZoomNow, &pose.zoom, 4)) return;
    remember(cameramemory::withNow(slotsNow(), pose, !g_startedOutdoor));
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

bool readHoverCursor(int& x, int& y) {
    const Asked asked(__builtin_return_address(0));
    return !uapad::runtime::look().shown && readGameCursor(x, y);
}

bool readGameCursor(int& x, int& y) {
    const Asked asked(__builtin_return_address(0));
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
constexpr uint32_t kPlayerFacing = 0x38;  // 3.0.11-beta.71: the actor's facing, degrees (FUN_00544da0, FUN_00580ca0)

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
    const Asked asked(__builtin_return_address(0));
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
    const Asked asked(__builtin_return_address(0));
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

namespace {

// 3.0.18-beta.166 (step 3; R-C8GVNT: netsmoothwin::beginScene up to 392 reads a frame - a walk of the world's actors
// for each pending move, the same for each HP bar and NPC dot): the world's actors and the pick list's records by
// their ids, read in one walk the first time a frame asks; each answer checked by the id at its place, the list read
// once more in the frame when a place no longer says it (or the id is not in it), then «none»
constexpr uint32_t kWorldActors = 0x18, kActorId = 0x208;
constexpr uint32_t kPickList = 0x74B5F0 + 0x28, kRecordId = 0x18;

struct IdIndex {
    uint32_t frame = ~0u, owner = 0;  // the frame it was read in and the world it is of (the pick list: 0)
    bool again = false;              // read once more this frame already
    std::unordered_map<uint32_t, uint32_t> byId;  // the id -> the actor's (the record's) place
};
uint32_t g_frame = 0;  // the presents so far (readTallyFrame)
IdIndex g_actors, g_picks;

// A list of the game's (a node: its next at +0, its object at +8) into `out` by the id at `idAt` of each object - the
// first one of an id kept, as a walk for one id found it
void walkIds(uint32_t headAt, uint32_t idAt, std::unordered_map<uint32_t, uint32_t>& out) {
    out.clear();
    uint32_t head = 0, node = 0;
    if (!read(headAt, &head, 4) || !head || !read(head, &node, 4)) return;
    for (size_t n = 0; node && node != head && n < 4096; ++n) {
        uint32_t object = 0, id = 0;
        if (!read(node + 8, &object, 4)) return;
        if (object && read(object + idAt, &id, 4) && id) out.emplace(id, object);
        if (!read(node, &node, 4)) return;
    }
}

uint32_t indexed(IdIndex& ix, uint32_t owner, uint32_t headAt, uint32_t idAt, uint32_t id) {
    const auto says = [&](uint32_t at) {
        uint32_t got = 0;
        return at && read(at + idAt, &got, 4) && got == id;
    };
    if (ix.frame != g_frame || ix.owner != owner) {
        ix.frame = g_frame;
        ix.owner = owner;
        ix.again = false;
        walkIds(headAt, idAt, ix.byId);
    }
    auto it = ix.byId.find(id);
    if (it != ix.byId.end() && says(it->second)) return it->second;
    if (ix.again) return 0;
    ix.again = true;
    walkIds(headAt, idAt, ix.byId);
    it = ix.byId.find(id);
    return it != ix.byId.end() && says(it->second) ? it->second : 0;
}

}  // namespace

uint32_t actorAddress(uint32_t id) {
    const Asked asked(__builtin_return_address(0));
    constexpr uint32_t kOwnAccount = 0x771F7C;
    uint32_t world = 0, own = 0;
    const uint32_t mode = gameMode();
    if (!id || !mode || !read(mode + kModeWorld, &world, 4) || !world) return 0;
    if (read(kOwnAccount, &own, 4) && own == id) {  // the character: the world's player, not in the list
        uint32_t player = 0;
        return read(world + kWorldPlayer, &player, 4) ? player : 0;
    }
    // 3.0.18-beta.166: by the frame's index of the world's actors (its place checked by the id there); the vtable
    // still the game's - 3.0.11-beta.272 (R-EP778D, a crash after a warp: an actor's freed memory still read its id,
    // its SendMsg called through garbage)
    const uint32_t actor = indexed(g_actors, world, world + kWorldActors, kActorId, id);
    return actor && gameObject(actor) ? actor : 0;
}

bool gameObject(uint32_t object) {
    const Asked asked(__builtin_return_address(0));
    uint32_t vtable = 0;
    return object && read(object, &vtable, 4) && vtable >= kGameRdataFrom && vtable < kGameRdataTo;
}

bool gameCode(uint32_t function) { return function >= kGameTextFrom && function < kGameTextTo; }

bool readActorWorld(uint32_t id, float at[3]) {
    const Asked asked(__builtin_return_address(0));
    const uint32_t actor = actorAddress(id);
    return actor && read(actor + 4, at, 3 * sizeof(float)) && std::isfinite(at[0]) && std::isfinite(at[1]) &&
           std::isfinite(at[2]);
}

void readActorPlaces(std::unordered_map<uint32_t, std::array<float, 3>>& out) {
    const Asked asked(__builtin_return_address(0));
    out.clear();
    uint32_t world = 0, head = 0, node = 0;
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + kModeWorld, &world, 4) || !world || !read(world + kWorldActors, &head, 4) || !head ||
        !read(head, &node, 4))
        return;
    for (size_t n = 0; node && node != head && n < 4096; ++n) {
        uint32_t actor = 0, id = 0;
        float at[3];
        if (!read(node + 8, &actor, 4)) return;
        if (actor && read(actor + kActorId, &id, 4) && id && read(actor + 4, at, sizeof at) && std::isfinite(at[0]) &&
            std::isfinite(at[1]) && std::isfinite(at[2]))
            out[id] = std::array<float, 3>{at[0], at[1], at[2]};
        if (!read(node, &node, 4)) return;
    }
}

int actorsOfId(uint32_t id, uint32_t actor, bool& listed) {
    const Asked asked(__builtin_return_address(0));
    listed = false;
    uint32_t world = 0, head = 0, node = 0;
    const uint32_t mode = gameMode();
    if (!id || !mode || !read(mode + kModeWorld, &world, 4) || !world || !read(world + kWorldActors, &head, 4) ||
        !head || !read(head, &node, 4))
        return -1;
    int count = 0;
    for (size_t n = 0; node && node != head && n < 4096; ++n) {
        uint32_t a = 0, aid = 0;
        if (!read(node + 8, &a, 4)) break;
        if (a && read(a + kActorId, &aid, 4) && aid == id) {
            ++count;
            if (a == actor) listed = true;
        }
        if (!read(node, &node, 4)) break;
    }
    return count;
}

bool readActorBox(uint32_t id, float box[4]) {
    const Asked asked(__builtin_return_address(0));
    if (!id || !gameMode()) return false;
    const uint32_t record = indexed(g_picks, 0, kPickList, kRecordId, id);  // 3.0.18-beta.166: the frame's index
    float f[5];
    if (!record || !read(record, f, sizeof f)) return false;
    box[0] = f[0];
    box[1] = f[1];
    box[2] = f[3];
    box[3] = f[4];
    return std::isfinite(box[0]) && std::isfinite(box[1]) && std::isfinite(box[2]) && std::isfinite(box[3]) &&
           box[2] > box[0] && box[3] > box[1];
}

void readActorBoxes(std::unordered_map<uint32_t, std::array<float, 4>>& out,
                    std::unordered_map<uint32_t, int>* classes) {
    const Asked asked(__builtin_return_address(0));
    out.clear();
    if (classes) classes->clear();
    constexpr uint32_t kRecordClass = 0x1C;
    uint32_t head = 0, node = 0;
    if (!gameMode() || !read(kPickList, &head, 4) || !head || !read(head, &node, 4)) return;
    for (size_t n = 0; node && node != head && n < 4096; ++n) {
        uint32_t record = 0, rid = 0;
        float f[5];
        if (!read(node + 8, &record, 4)) return;
        if (record && read(record + kRecordId, &rid, 4) && rid && read(record, f, sizeof f) && std::isfinite(f[0]) &&
            std::isfinite(f[1]) && std::isfinite(f[3]) && std::isfinite(f[4]) && f[3] > f[0] && f[4] > f[1]) {
            out[rid] = std::array<float, 4>{f[0], f[1], f[3], f[4]};
            uint32_t cls = 0;
            if (classes && read(record + kRecordClass, &cls, 4)) (*classes)[rid] = int(cls);
        }
        if (!read(node, &node, 4)) return;
    }
}

bool readPlayerFacing(float& degrees) {
    const Asked asked(__builtin_return_address(0));
    uint32_t world = 0, player = 0;
    const uint32_t mode = gameMode();
    return mode && read(mode + kModeWorld, &world, 4) && world && read(world + kWorldPlayer, &player, 4) && player &&
           read(player + kPlayerFacing, &degrees, sizeof degrees) && std::isfinite(degrees);
}

bool gameMinimapShown() {
    const Asked asked(__builtin_return_address(0));
    uint32_t window = 0;
    return gameMode() && read(kMinimapWindow, &window, 4) && window != 0;
}

bool hideGameMinimap() {
    const Asked asked(__builtin_return_address(0));
    if (!knownWindowCalls() || !gameMinimapShown()) return false;
    reinterpret_cast<WindowFn>(uintptr_t(kDeleteWindow))(reinterpret_cast<void*>(uintptr_t(kWindowMgr)), kMinimapId);
    return !gameMinimapShown();
}

bool showGameMinimap() {
    const Asked asked(__builtin_return_address(0));
    uint32_t allowed = 0;
    const uint32_t mode = gameMode();
    if (!knownWindowCalls() || !mode || gameMinimapShown() || !read(mode + kModeMinimapAllowed, &allowed, 4) ||
        !allowed)
        return false;
    reinterpret_cast<WindowFn>(uintptr_t(kMakeWindow))(reinterpret_cast<void*>(uintptr_t(kWindowMgr)), kMinimapId);
    return gameMinimapShown();
}

std::string readGameMap() {
    const Asked asked(__builtin_return_address(0));
    char name[48] = {};
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + camera::kModeMap, name, sizeof name - 1)) return "";
    return ai::mapName(name, sizeof name - 1);
}

std::string readOwnName() {
    const Asked asked(__builtin_return_address(0));
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
    const Asked asked(__builtin_return_address(0));
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

bool pointerInTarget(HWND game, UINT m, LPARAM lp, double k, int targetWidth, int targetHeight, float& tx, float& ty,
                     bool* fromGame) {
    int cx = 0, cy = 0;
    if (k > 0 && readGameCursor(cx, cy)) {
        tx = float(double(cx) * k);
        ty = float(double(cy) * k);
        if (fromGame) *fromGame = true;
        return true;
    }
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (m == WM_MOUSEWHEEL && !ScreenToClient(game, &p)) return false;  // the wheel's point is the screen's
    RECT client{};
    if (!GetClientRect(game, &client) || client.right <= 0 || client.bottom <= 0) return false;
    tx = float(p.x) * float(targetWidth) / float(client.right);  // Windows' size of the window (not DPI-aware)
    ty = float(p.y) * float(targetHeight) / float(client.bottom);
    if (fromGame) *fromGame = false;
    return true;
}

void readTallyFrame(uint32_t gameThread) {
    if (!g_msPerTick) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        g_msPerTick = f.QuadPart ? 1000.0 / double(f.QuadPart) : 0.0;
    }
    g_tallyThread = gameThread;
    g_tally.frame();
    ++g_frame;  // 3.0.18-beta.166: the actors' and the pick list's indexes read again at the next frame's first ask
}

std::string takeReadTally() {
    HMODULE self = nullptr;  // the driver's own module (the system's ddraw.dll may be loaded under the same name)
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       reinterpret_cast<LPCWSTR>(&takeReadTally), &self);
    const uint32_t base = uint32_t(uintptr_t(self));
    // 3.0.18-beta.163: the places summed by the driver's functions (its unwind table, read once - the module stays)
    static std::vector<pe::Function> functions;
    static bool read = false;
    if (!read && base) {
        read = true;
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(self);
        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS32*>(uintptr_t(base) + uintptr_t(dos->e_lfanew));
        const uint32_t size = nt->OptionalHeader.SizeOfImage;
        functions = pe::functionsOf([base, size](uint32_t rva, void* out, size_t n) {
            if (rva > size || n > size - rva) return false;
            std::memcpy(out, reinterpret_cast<const void*>(uintptr_t(base) + rva), n);
            return true;
        });
    }
    return g_tally.take(base, 6, functions.empty() ? nullptr : &functions);
}

}  // namespace uagfx::win
