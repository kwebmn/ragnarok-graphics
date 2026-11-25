#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>

namespace uagfx::res_tree_exe {

// 3.0.23-beta.4 (R-WS4ETW twice, R-NE7JVF: an access violation reading 0 at uaRO.exe+0x112783/0x112789 right after
// malangdo's load, 3.0.22 and 3.0.23-beta.2; the dump's stack the map model loader thread's (FUN_004ea130) in
// CResMgr::Get 0x513770): the resource cache keeps a std::map per file kind (CResMgr +0x34, 16 bytes each); Get finds a
// name by its lower_bound 0x517160, which compares keys by 0x512780 (a key: a word and the name after it). Another
// thread (the game's own, loading the same map) was putting a node in - its key not written yet - and the walk read it.
// Both calls of the lower_bound go to ours: the same walk, a node with no key (or no key asked) - not found, so Get
// loads the file as for a new one, instead of the fault.
constexpr uint32_t kLowerBound = 0x517160;
constexpr uint32_t kLess = 0x512780;      // thiscall: (this = a node's key, the key asked) -> the node's is less
constexpr uint32_t kNilAt = 0x74BB98;     // the trees' nil node
constexpr uint32_t kCalls[2] = {0x513799, 0x5142D9};  // CResMgr's two «call 0x517160»
constexpr size_t kFnSize = 0x4B;
extern const uint8_t kFn[kFnSize];        // this build's lower_bound, byte for byte

// The game's walk with the guard: a node +0 left, +8 right, +0xc its key's address; the map +4 its head, the head +4
// its root. `broken` set when a node with no key (or no key asked) ended it
uint32_t lowerBound(uint32_t map, uint32_t keyRef, uint32_t nil, const std::function<uint32_t(uint32_t)>& read,
                    const std::function<bool(uint32_t, uint32_t)>& less, bool& broken);
// «call target» written at `site`
void callTo(uint32_t site, uint32_t target, uint8_t out[5]);

}  // namespace uagfx::res_tree_exe
