#include "win/dx7/com.hpp"

#include <ddraw.h>

#include <set>
#include <string>

#include "win/log.hpp"

namespace uagfx::dx7 {
namespace {

struct Seen {
    CRITICAL_SECTION lock;
    std::set<std::string> names;
    Seen() { InitializeCriticalSection(&lock); }
};

Seen& seen() {
    static Seen s;
    return s;
}

}  // namespace

HRESULT notImplemented(const char* method) {
    Seen& s = seen();
    EnterCriticalSection(&s.lock);
    const bool first = s.names.insert(method).second;
    LeaveCriticalSection(&s.lock);
    if (first) LOG_INFO("not implemented: %s (answered DDERR_UNSUPPORTED)", method);
    return DDERR_UNSUPPORTED;
}

}  // namespace uagfx::dx7
#include "win/dx7/com_bases.hpp"  // compiled here so a header the MinGW headers disagree with fails the build early
