#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// 3.0.20-beta.76 (the owner's log of 16.11: the 64-bit renderer silent for five seconds at a map's first frames, then
// gone with exit code 0xC0000409 - a fast fail leaves no dump and runs no handler of ours): where a thread stands,
// read from its stack's words alone - no unwinding, no locks, so it can be taken of a thread held still for a
// moment. A word is kept when it points into a loaded module's image and the bytes before it there are a call's:
// a return address (a few stale ones of earlier calls come with them - the order is the stack's, the newest first).
namespace uagfx::stackscan {

struct Module {
    uint64_t base = 0, size = 0;
    std::string name;
};
struct Frame {
    int module = -1;  // its index in the modules
    uint64_t rva = 0;
};
// the image's bytes at `at`: false when they cannot be read
using ReadFn = bool (*)(void* context, uint64_t at, uint8_t* out, size_t size);

// whether the bytes before `at` are an x86-64 call's (E8 rel32; FF /2 by a register, a register's memory with no,
// a short or a long displacement, with or without SIB; the RIP-relative one)
bool afterCall(uint64_t at, ReadFn read, void* context);
// the stack's words that are return addresses into the modules, the top's first, `most` at most
std::vector<Frame> scan(const uint64_t* words, size_t count, const std::vector<Module>& modules, ReadFn read,
                        void* context, size_t most);
std::string place(uint64_t address, const std::vector<Module>& modules);  // "name+0x1a2b"; no module's: "0x..."
std::string calls(const std::vector<Frame>& frames, const std::vector<Module>& modules);  // "a+0x1 < b+0x2"

}  // namespace uagfx::stackscan
