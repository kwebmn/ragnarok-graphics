#pragma once
#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace uagfx::app {

// The pack's writer for as long as it lives: the builder's mutex (a builder or another change: busy) and the pack
// writer's (a game making pictures: busy). 2.5.28: uaRO Studio's, shared with the settings app's Texture pack page.
class PackLock {
public:
    bool take(std::string& error);
    ~PackLock();

private:
    HANDLE builder_ = nullptr, writer_ = nullptr;
    bool held_ = false;
};

// uaRO Studio's P1, the Texture pack page's 2.5.28: the pack copied whole into a new file without the space replaced
// and taken-out pictures hold, put in its place; gameOpen: why when a running game holds the pack open. progress: the
// pictures looked at of all of them.
bool compactPack(const std::string& pack, std::string& error, const std::string& gameOpen,
                 const std::function<void(size_t done, size_t total)>& progress = {});

// 2.5.28: the Texture pack page's Compact on a thread of its own - its state read by the page while it works.
struct CompactState {
    enum Kind { Idle, Working, Done, Failed } kind = Idle;
    size_t done = 0, total = 0;  // pictures looked at, of all
    uint64_t freed = 0;          // Done: bytes the pack is smaller by
    std::string error;           // Failed: why, in English
};
// needFree: bytes the disk must have free for the copy; gameOpen as compactPack's
void startCompact(const std::wstring& pack, uint64_t needFree, const std::string& gameOpen);
CompactState compactState();
bool compactRunning();

}  // namespace uagfx::app
