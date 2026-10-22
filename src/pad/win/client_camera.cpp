#include "pad/win/client_camera.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

#include "pad/win/client_memory.hpp"

namespace uapad::clientcamera {
namespace {

using clientmem::load;

// uaRO.exe / uaRO-1.1.exe: the mode manager and, in game mode, CGameMode and its CView.
constexpr uintptr_t kModeMgr = 0x74b578;
constexpr uintptr_t kModeNow = kModeMgr + 0x4;   // the current mode
constexpr uintptr_t kModeKind = kModeMgr + 0x58;  // 1: the game itself
constexpr uintptr_t kModeView = 0xc8;             // CGameMode: its CView
constexpr uintptr_t kModeLimited = 0x284;         // non-zero: the turn stays within the shorts below
constexpr uintptr_t kModeTurnMin = 0x28e;
constexpr uintptr_t kModeTurnMax = 0x290;
constexpr uintptr_t kModeFree = 0x280;            // non-zero: free turning; zero: 20 degrees around kModeHome
constexpr uintptr_t kModeHome = 0x2b8;            // float
constexpr uintptr_t kViewTurnNow = 0x20;          // CView: the turn on screen
constexpr uintptr_t kViewTurnTo = 0x38;           // the turn it heads for: what the drag changes
constexpr float kHomeSpan = 20.0f;                // 0x69f174

// CModeMgr::GetGameMode; a caller with the manager's address; the right-button drag using the mode's view,
// its turn limits and the view's turn (0x5660e9..0x566183).
const uint8_t kSigGetGameMode[] = {0x83, 0x79, 0x58, 0x01, 0x75, 0x04, 0x8b, 0x41, 0x04, 0xc3, 0x33, 0xc0, 0xc3};
const uint8_t kSigManager[] = {0xb9, 0x78, 0xb5, 0x74, 0x00, 0xe8, 0xdf, 0x36, 0xea, 0xff};
const uint8_t kSigDrag[] = {
    0x8b, 0x86, 0x84, 0x02, 0x00, 0x00, 0x85, 0xc0, 0x74, 0x5b, 0x8b, 0x8e, 0xc8, 0x00, 0x00, 0x00, 0x0f, 0xbf, 0x86,
    0x8e, 0x02, 0x00, 0x00, 0xd9, 0x41, 0x38, 0xda, 0x05, 0x04, 0xe7, 0x6e, 0x00, 0x89, 0x45, 0xfc, 0xda, 0xa6, 0xd4,
    0x00, 0x00, 0x00, 0xdb, 0x45, 0xfc, 0xd9, 0x5d, 0xfc, 0xd8, 0x55, 0xfc, 0xdf, 0xe0, 0xf6, 0xc4, 0x01, 0x74, 0x05,
    0xdd, 0xd8, 0xd9, 0x45, 0xfc, 0x0f, 0xbf, 0x96, 0x90, 0x02, 0x00, 0x00, 0x89, 0x55, 0xfc, 0xdb, 0x45, 0xfc, 0xd9,
    0x5d, 0xfc, 0xd8, 0x55, 0xfc, 0xdf, 0xe0, 0xf6, 0xc4, 0x41, 0x75, 0x05, 0xdd, 0xd8, 0xd9, 0x45, 0xfc, 0xd9, 0x59,
    0x38, 0xe9, 0x92, 0x00, 0x00, 0x00, 0x8b, 0x86, 0x80, 0x02, 0x00, 0x00, 0x85, 0xc0, 0x75, 0x66, 0x6a, 0x06, 0xb9,
    0x48, 0x11, 0x77, 0x00, 0xe8, 0x3c, 0xe4, 0x0c, 0x00, 0x85, 0xc0, 0x75, 0x56, 0x8b, 0x8e, 0xc8, 0x00, 0x00, 0x00,
    0xd9, 0x41, 0x38, 0xda, 0x05, 0x04, 0xe7, 0x6e, 0x00, 0xda, 0xa6, 0xd4, 0x00, 0x00, 0x00, 0xd9, 0x86, 0xb8, 0x02,
    0x00, 0x00};

bool gameView(uintptr_t& mode, uintptr_t& view) {
    int32_t kind = 0;
    uint32_t m = 0;
    uint32_t v = 0;
    if (!load(kModeKind, kind) || kind != 1 || !load(kModeNow, m) || m == 0 || !load(m + kModeView, v) || v == 0)
        return false;
    mode = m;
    view = v;
    return true;
}

// `turn` brought within `span` of `centre`, measured the short way round.
float within(float turn, float centre, float span) {
    const float off = std::remainder(turn - centre, 360.0f);
    return centre + std::clamp(off, -span, span);
}

}  // namespace

bool supported() {
    return clientmem::matches(kSigGetGameMode, sizeof kSigGetGameMode, 0x4f6ad0) &&
           clientmem::matches(kSigManager, sizeof kSigManager, 0x6533e7) &&
           clientmem::matches(kSigDrag, sizeof kSigDrag, 0x5660e9);
}

bool readTurn(double& degrees) {
    uintptr_t mode = 0;
    uintptr_t view = 0;
    float now = 0;
    if (!gameView(mode, view) || !load(view + kViewTurnNow, now) || !std::isfinite(now)) return false;
    degrees = now;
    return true;
}

bool turn(float deltaDeg) {
    uintptr_t mode = 0;
    uintptr_t view = 0;
    float to = 0;
    int32_t limited = 0;
    int32_t free = 0;
    if (!gameView(mode, view) || !load(view + kViewTurnTo, to) || !std::isfinite(to) ||
        !load(mode + kModeLimited, limited) || !load(mode + kModeFree, free))
        return false;
    float next = to + deltaDeg;
    if (limited != 0) {
        int16_t lo = 0;
        int16_t hi = 0;
        if (!load(mode + kModeTurnMin, lo) || !load(mode + kModeTurnMax, hi) || hi < lo) return false;
        next = within(next, (float(lo) + float(hi)) / 2.0f, (float(hi) - float(lo)) / 2.0f);
    } else if (free == 0) {
        float home = 0;
        if (!load(mode + kModeHome, home) || !std::isfinite(home)) return false;
        next = within(next, home, kHomeSpan);
    }
    if (!clientmem::writable(view + kViewTurnTo, sizeof next)) return false;
    std::memcpy(reinterpret_cast<void*>(view + kViewTurnTo), &next, sizeof next);
    return true;
}

}  // namespace uapad::clientcamera
