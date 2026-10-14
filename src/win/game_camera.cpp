#include "win/game_camera.hpp"

#include <windows.h>

#include <cstdint>

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

}  // namespace

bool readGameCamera(float view[12]) {
    static int build = -1;  // known once: the exe does not change while it runs
    if (build < 0) {
        float constants[5];
        build = read(camera::kConstants, constants, sizeof constants) && camera::knownConstants(constants) ? 1 : 0;
    }
    uint32_t type = 0, mode = 0, cview = 0;
    if (!build || !read(camera::kModeMgr + camera::kModeType, &type, 4) || type != 1 ||
        !read(camera::kModeMgr + camera::kModeCurrent, &mode, 4) || !read(mode + camera::kModeView, &cview, 4) ||
        !read(cview + camera::kViewMatrix, view, 12 * sizeof(float)))
        return false;
    return camera::plausibleView(view);
}

}  // namespace uagfx::win
