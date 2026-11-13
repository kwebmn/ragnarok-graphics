#pragma once
#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>

namespace uagfx::updates {

// 3.0.2-beta.5 (the user: the admin's new content packed into a GRF of his own, «grf в отдельную папку»): the GRFs
// in <client>\updates\ over the game's archives - for the game (win/updates_hook.cpp) and the driver's own readers
// (docs/superpowers/specs/2026-10-23-graphics-updates-design.md). The site's «Оновлення клієнта» brings them.
constexpr const char* kFolder = "updates";

// A game file's name as the index knows it: ASCII letters in lower case, '/' as '\'; a CP949 character's second byte
// (it may be an ASCII letter's code) kept as it is
std::string lookupKey(const std::string& name);
// The folder's GRFs in the order they are looked in: by name, the later one first (a later update wins)
std::vector<std::string> lookupOrder(std::vector<std::string> files);
// The archives the driver's own readers look in after the loose files: the folder's GRFs (as lookupOrder, with the
// folder before them), then DATA.INI's in their order
std::vector<std::string> archiveOrder(const std::vector<std::string>& files, const std::vector<std::string>& dataIni);

// 3.0.18-beta.251 (R-NGSWJM; the castles' crash «не можу попасти в замок», R-FPT4KM): uaRO.exe reads each archive
// through one window of it (FUN_00510c20: 512 MB, halved while that fails - 256 MB here) and maps another for a file
// past it. Its map loader thread (FUN_004ea130, under its resource lock) and the game's thread (a castle's 3D models,
// CFile::Open without that lock) read at once, and one's window went under the other's inflate (data.grf 1.7 GB, the
// WoE models in three windows). The driver's hook on CFileMgr::GetData takes its reads one at a time - always, with
// updates or without. What to hook: the game's GetData known (its first bytes), [client] updates on, the folder's
// GRFs, the game's lookups known
struct HookPlan {
    bool inTurn = false;   // GetData hooked, its reads one at a time
    bool updates = false;  // the folder's GRFs over the game's archives (GetData, IsExist, CFile::IsExist)
};
HookPlan hookPlan(bool getDataKnown, bool updatesOn, size_t files, bool lookupsKnown);
// The n-th read (from 1) that waited for another thread's: told in the log (the first five, then every hundredth)
bool tellWait(unsigned n);

struct Hit {
    int archive;       // in the order they were added
    std::string name;  // as the archive stores it
};

// The names of the update archives: the first archive added (the later update) wins a name
class Index {
public:
    void add(int archive, const std::vector<std::string>& names);
    const Hit* find(const std::string& name) const;
    size_t size() const { return hits_.size(); }

private:
    std::unordered_map<std::string, Hit> hits_;
};

}  // namespace uagfx::updates
