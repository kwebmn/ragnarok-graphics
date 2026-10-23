#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <utility>
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

// 3.0.2-beta.12 (the client build session: prt_fild08 loads in 3.7 s, cps.dll's inflating 6 times as fast changed
// nothing - where does the game's thread spend a map's loading?): the steps of uaRO.exe a sample can be under, by
// this build's functions (2007-05-07, the decompilation): its files read from the GRFs (CFileMgr::GetData 0x50d120,
// the lookup 0x510f20, the read with its DES and inflating 0x510fd0); the textures' pictures (0x49d830; .bmp, .tga
// and .jpg: the picture's class 0x42b030-0x42c1a0); sprites (CSprRes::Load 0x4385f0); the resources made of their
// files (0x5131f0: models, the map's files); the world and its near models (the map's load 0x4ea9a0); the rest of
// the map's start (0x559030); showing a frame (0x40fdb0, 0x404bc0); the server's packets (0x579900); Windows'
// messages (0x654100); the rest of a game's frame (CGameMode::OnRun 0x559750). 3.0.2-beta.13: the cell under the
// mouse (the game's pick 0x564a20 and its cell test 0x42a9a0, which the driver's own pick calls too).
enum class Step : int {
    Files,
    Pictures,
    Sprites,
    Resources,
    World,
    MapStart,
    Present,
    Packets,
    Messages,
    Frames,
    Pick,
    Count
};

// The innermost step a sample is under - its own place first, then its return addresses outward (uaRO.exe's
// addresses; others are under none); -1 under none. Only for this build of the exe.
int stepOf(uint32_t at, const uint32_t* returns, size_t count);
const char* stepName(int step);  // "elsewhere" for -1

// 3.0.2-beta.12: a sample with what it is besides its place - `where`, its time in Windows' code by the function the
// game or one of its folder's libraries called ("Sleep", "ReadFile", "WaitForSingleObject from ddraw.dll"; "": the
// module's name), and the game's step it is under (-1 none).
struct Sample {
    uint32_t at = 0;
    uint32_t caller = 0;  // outside uaRO.exe: the uaRO.exe address it was called from, 0 unknown
    std::string where;
    int step = -1;
};

class Tally {
public:
    // exe: the index of uaRO.exe in them; self (2.5.5): the driver's own module's, tallied by place as offsets from
    // its base (its functions found in the build's link map afterwards); none: past the list
    void setModules(std::vector<Module> modules, size_t exe, size_t self = size_t(-1));
    // A sample: where the thread was, and (outside uaRO.exe) the uaRO.exe address it was called from, 0 unknown.
    void add(uint32_t at, uint32_t caller);
    void add(const Sample& s);  // 3.0.2-beta.12
    // The lines for the log, the tally then cleared. seconds: the time the samples were taken over.
    std::vector<std::string> report(double seconds, size_t top = 15);
    // 3.0.2-beta.12: the same lines under a prefix of their own ("map prontera"), the first with `head`'s words before
    // its tally ("map prontera: loaded in 3.69 s - Sleep 50.0%, ...")
    std::vector<std::string> report(const std::string& prefix, const std::string& head, size_t top = 15);
    size_t samples() const { return samples_; }

private:
    int moduleOf(uint32_t at) const;
    std::vector<Module> modules_;
    size_t exe_ = 0;
    size_t samples_ = 0;
    std::map<std::string, size_t> byWhere_;    // the module's name, or Windows' function (3.0.2-beta.12)
    std::map<uint32_t, size_t> inExe_;         // 64-byte spans of uaRO.exe
    std::map<std::pair<uint32_t, std::string>, size_t> callers_;  // the caller's 16-byte span, where it went
    size_t self_ = size_t(-1);
    std::map<uint32_t, size_t> inSelf_;        // 2.5.5: 64-byte spans of the driver's own module, from its base
    std::map<int, size_t> bySteps_;            // 3.0.2-beta.12: -1 under none
    size_t stepped_ = 0;                       // the samples under a step
};

}  // namespace uagfx::profile
