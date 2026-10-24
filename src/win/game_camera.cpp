#include "win/game_camera.hpp"

#include <windows.h>

#include <cmath>
#include <cstdint>
#include <cstring>

#include "core/ai/texture_cache.hpp"
#include "core/char_name.hpp"
#include "core/game_camera.hpp"
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

bool readGameCursor(int& x, int& y) {
    int32_t at[2] = {0, 0};
    if (!knownBuild() || !read(camera::kMouse + camera::kMouseX, &at[0], 4) ||
        !read(camera::kMouse + camera::kMouseY, &at[1], 4))
        return false;
    x = at[0];
    y = at[1];
    return true;
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
