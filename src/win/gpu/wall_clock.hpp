#pragma once
#include <windows.h>

namespace uagfx::gpu {

// 3.0.22-beta.23: the wall clock in seconds since 1970 (UTC) - the sandstorm's moments, the same on every computer
// (core weather::stormAt); GetSystemTimeAsFileTime: every Windows the client runs on (the precise one wants 8)
inline double wallSeconds() {
    FILETIME ft;
    GetSystemTimeAsFileTime(&ft);
    const unsigned long long t = (static_cast<unsigned long long>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
    return double(t - 116444736000000000ull) / 1.0e7;
}

}  // namespace uagfx::gpu
