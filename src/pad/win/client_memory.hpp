#pragma once
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

// Reading the client's own memory: what is safe to read, and whether its code is the build we know.
namespace uapad::clientmem {

// True when [p, p + n) is committed, readable memory of this process.
bool readable(uintptr_t p, size_t n);

template <class T>
bool load(uintptr_t p, T& out) {
    if (!readable(p, sizeof(T))) return false;
    std::memcpy(&out, reinterpret_cast<const void*>(p), sizeof(T));
    return true;
}

// True when [p, p + n) is committed memory of this process that can be written.
bool writable(uintptr_t p, size_t n);

// The client's code at `at` is byte for byte `sig`.
bool matches(const uint8_t* sig, size_t n, uintptr_t at);

}  // namespace uapad::clientmem
