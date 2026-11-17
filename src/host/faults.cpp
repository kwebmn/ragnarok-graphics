#include "host/faults.hpp"

#include <windows.h>

#include <atomic>
#include <cstdint>

#include "host/hang_watch.hpp"
#include "win/crash.hpp"
#include "win/log.hpp"

namespace uagfx::host::faults {
namespace {

using InvalidFn = void (*)(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t);
using SetInvalidFn = InvalidFn (*)(InvalidFn);
constexpr int kMostTold = 5;
std::atomic<int> g_told{0};

void onInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t) {
    const int n = ++g_told;
    if (n > kMostTold) return;
    LOG_ERROR("fault: msvcrt.dll was handed a wrong value (an invalid parameter: Windows would end the renderer with "
              "exit code 0xC0000409) - the call refused; the calls on this thread's stack, the newest first: %s",
              hangwatch::callsHere().c_str());
    if (n == 1 && win::writeHangDump()) LOG_ERROR("fault: a minidump is in uaro-graphics\\crash-render64.dmp");
}

}  // namespace

void install() {
    const HMODULE crt = GetModuleHandleA("msvcrt.dll");
    const auto set = crt ? reinterpret_cast<SetInvalidFn>(
                               reinterpret_cast<void*>(GetProcAddress(crt, "_set_invalid_parameter_handler")))
                         : nullptr;
    if (!set) {
        LOG_INFO("faults: msvcrt.dll has no _set_invalid_parameter_handler - a wrong value handed to it still ends the "
                 "renderer");
        return;
    }
    set(&onInvalidParameter);
    LOG_INFO("faults: a wrong value handed to msvcrt.dll is told with the calls on the stack (and a dump), not the "
             "renderer's end");
}

}  // namespace uagfx::host::faults
