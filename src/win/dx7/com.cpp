#include "win/dx7/com.hpp"

#include <ddraw.h>

#include <cstdio>
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

void unknownInterface(const char* object, REFIID riid) {
    char name[112];
    std::snprintf(name, sizeof name, "%s::QueryInterface {%08lx-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x}",
                  object, static_cast<unsigned long>(riid.Data1), riid.Data2, riid.Data3, riid.Data4[0],
                  riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6],
                  riid.Data4[7]);
    Seen& s = seen();
    EnterCriticalSection(&s.lock);
    const bool first = s.names.insert(name).second;
    LeaveCriticalSection(&s.lock);
    if (first) LOG_INFO("%s: E_NOINTERFACE", name);
}

}  // namespace uagfx::dx7
#include "win/dx7/com_bases.hpp"  // compiled here so a header the MinGW headers disagree with fails the build early
