#pragma once
#include <cstddef>
#include <cstdint>

// 3.0.18-beta.165 (the owner's «делай» - step 3, the driver's work with the game; R-MBCDBU: 870-1460 reads of the
// game's memory a frame, each a ReadProcessMemory on ourselves of about 0.9 us - 0.8-1.3 ms a frame): the copy made by
// the processor itself. A byte that is not mapped faults, and a vectored handler of ours sends the copy on to its
// «false» instead of the game's crash - a fault comes only for an address gone, so a read costs no call into Windows.
namespace uagfx::win::saferead {

// The handler in place (added the first time asked, then checked once: a page with no access must answer false, a
// known value true). False: the callers keep ReadProcessMemory.
bool ready();
// `size` bytes from the game's `from` into `out`; false when one of them is not readable (out partly written then).
// Only when ready().
bool copy(void* out, uint32_t from, size_t size);
// 3.0.18-beta.166: the same copy the other way - `size` bytes into the game's data at `to` (its heap: an actor's place,
// not its code - a page that is not writable answers false where WriteProcessMemory would change its protection).
// Only when ready().
bool put(uint32_t to, const void* in, size_t size);
// [at, at + size) readable now: a byte of each page it touches tried by that copy; without ready() by VirtualQuery
// (committed, readable, no guard) as the callers did before - one place for what win/texture_hook, win/crisp and
// pad/win/client_memory each had
bool readable(uint32_t at, size_t size);
// The copies that faulted (an address gone) since the last take - each fault costs Windows' exception dispatch, so
// many a frame would be a module reading stale places (the driver's 30 s line tells them)
unsigned takeFaults();

}  // namespace uagfx::win::saferead
