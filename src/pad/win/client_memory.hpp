#pragma once
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <cstring>

// Reading the client's own memory: what is safe to read, and whether its code is the build we know.
namespace uapad::clientmem {

// True when [p, p + n) is committed, readable memory of this process.
bool readable(uintptr_t p, size_t n);

// 3.0.18-beta.165: [p, p + n) into `out` - the processor's own copy with the driver's fault handler
// (win/safe_read: no call into Windows), else checked by readable() first as before
bool copy(uintptr_t p, void* out, size_t n);

template <class T>
bool load(uintptr_t p, T& out) {
    return copy(p, &out, sizeof(T));
}

// True when [p, p + n) is committed memory of this process that can be written.
bool writable(uintptr_t p, size_t n);

// 3.0.18-beta.161: the checks above counted on the game's thread (each a VirtualQuery) - the driver's 30 s line takes
// their count and time, the count then started again
void countOn(DWORD gameThread);
void takeQueries(uint32_t& count, double& ms);

// The client's code at `at` is byte for byte `sig`.
bool matches(const uint8_t* sig, size_t n, uintptr_t at);

}  // namespace uapad::clientmem
