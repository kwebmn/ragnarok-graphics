#include "win/pad_mount.hpp"

#include "core/config.hpp"
#include "pad/win/mount.hpp"
#include "win/paths.hpp"

namespace uagfx::win {
namespace {

uapad::Mounted g_pad = uapad::Mounted::Off;

}  // namespace

void mountPad() {
    std::string text;
    readTextFile(ourFile(layout::File::Ini), text);  // the 2.x root file when the driver has not moved it yet
    g_pad = uapad::mount(parseConfig(text).padEnabled);
}

void unmountPad() { uapad::unmount(); }

const char* padMountedText() { return uapad::mountedText(g_pad); }

}  // namespace uagfx::win
