#include "win/real_ddraw.hpp"

#include <string>

#include "core/version.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win {
namespace {

HMODULE loadReal() {
    const std::string dir = gameDir();
    wlog::open(dir + "\\uaro-graphics.log", LogLevel::Info);
    LOG_INFO("uaRO Graphics %s in %s", kVersion, modulePath(GetModuleHandleA(nullptr)).c_str());
    const std::string path = systemDir() + "\\ddraw.dll";
    HMODULE m = LoadLibraryA(path.c_str());
    if (m) {
        LOG_INFO("passing DirectDraw to %s", path.c_str());
    } else {
        LOG_ERROR("cannot load %s (error %lu)", path.c_str(), GetLastError());
    }
    return m;
}

}  // namespace

HMODULE realModule() {
    static HMODULE m = loadReal();
    return m;
}

FARPROC realProc(const char* name) {
    HMODULE m = realModule();
    return m ? GetProcAddress(m, name) : nullptr;
}

}  // namespace uagfx::win
