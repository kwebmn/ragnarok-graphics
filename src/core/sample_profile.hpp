#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace uagfx::profile {

// 0.87, [debug] profile: where the game's thread spends its time - the driver samples it about a thousand times a
// second (win/profiler.cpp) and this tallies the samples: by module, by place in uaRO.exe (64-byte spans: a place in
// a function, matched to the decompilation afterwards), and for the time spent in other modules (Windows, the graphics
// driver, ours) the place in uaRO.exe that called into them.
struct Module {
    std::string name;
    uint32_t base = 0, size = 0;
};

class Tally {
public:
    void setModules(std::vector<Module> modules, size_t exe);  // exe: the index of uaRO.exe in them
    // A sample: where the thread was, and (outside uaRO.exe) the uaRO.exe address it was called from, 0 unknown.
    void add(uint32_t at, uint32_t caller);
    // The lines for the log, the tally then cleared. seconds: the time the samples were taken over.
    std::vector<std::string> report(double seconds, size_t top = 15);
    size_t samples() const { return samples_; }

private:
    int moduleOf(uint32_t at) const;
    std::vector<Module> modules_;
    size_t exe_ = 0;
    size_t samples_ = 0;
    std::map<int, size_t> byModule_;           // -1: no module (generated code, the stack)
    std::map<uint32_t, size_t> inExe_;         // 64-byte spans of uaRO.exe
    std::map<uint64_t, size_t> callers_;       // caller span << 32 | module
};

}  // namespace uagfx::profile
