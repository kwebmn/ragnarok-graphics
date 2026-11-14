#pragma once
#include <cstdint>
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
// 3.0.19-beta.1 (the testers: the old client - «CPaletteRes :: Cannot find File : palette\몸\프리스트_남_4.pal», then
// uaRO_New.exe at 0x417B94, every account's character select; the game loads a dye's palette there unchecked): the
// sprite a dye's palette recolours - data\palette\몸\<job>_<sex>_<n>.pal: data\sprite\인간족\몸통\<sex>\<job>_<sex>.spr,
// data\palette\머리\머리<style>_<sex>_<n>.pal: data\sprite\인간족\머리통\<sex>\<style>_<sex>.spr (CP949 bytes, the
// ASCII parts in any case); "" another name
std::string paletteSprite(const std::string& name);
// A sprite's own palette - an SPR 1.1 or later keeps it in its last 1024 bytes; false: none
bool spritePalette(const std::vector<uint8_t>& spr, std::vector<uint8_t>& palette);

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
