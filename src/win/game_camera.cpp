#include "win/game_camera.hpp"

#include <windows.h>

#include <cmath>
#include <cstdint>

#include "core/ai/texture_cache.hpp"
#include "core/game_camera.hpp"

namespace uagfx::win {
namespace {

// ReadProcessMemory on ourselves: an address that is not mapped fails instead of faulting.
bool read(uint32_t address, void* out, size_t size) {
    SIZE_T got = 0;
    return address && ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(uintptr_t(address)), out,
                                        size, &got) &&
           got == size;
}

// The game mode while it shows a world; 0 at the login and the character list, or in another build of the exe.
uint32_t gameMode() {
    static int build = -1;  // known once: the exe does not change while it runs
    if (build < 0) {
        float constants[5];
        build = read(camera::kConstants, constants, sizeof constants) && camera::knownConstants(constants) ? 1 : 0;
    }
    uint32_t type = 0, mode = 0;
    if (!build || !read(camera::kModeMgr + camera::kModeType, &type, 4) || type != 1 ||
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

std::string readGameMap() {
    char name[48] = {};
    const uint32_t mode = gameMode();
    if (!mode || !read(mode + camera::kModeMap, name, sizeof name - 1)) return "";
    return ai::mapName(name, sizeof name - 1);
}

}  // namespace uagfx::win
