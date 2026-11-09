#include "core/settings/game_key.hpp"

#include <cstdint>
#include <cstdio>

#include "core/client_patch.hpp"

namespace uagfx::settings {
namespace {

std::string lower(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return s;
}

const std::string kBase = "Software\\Gravity Soft\\";

}  // namespace

std::string gameKeyLeaf(const std::string& client) {
    if (client.empty() || client_patch::sameClient(client, client_patch::kOldClient)) return kSharedLeaf;
    bool fits = kBase.size() + client.size() <= kGameKeyRoom;
    for (const char c : client) fits = fits && c >= 0x20 && c <= 0x7E && c != '\\';
    const std::string l = lower(client);
    fits = fits && l != "ragnarok" && l != "roclient";
    if (fits) return client;
    uint32_t h = 2166136261u;  // FNV-1a of the name as written
    for (const char c : client) h = (h ^ uint8_t(c)) * 16777619u;
    char tail[8];
    std::snprintf(tail, sizeof tail, "%04X", unsigned(h & 0xFFFF));
    return std::string("uaRO-") + tail;
}

std::string gameKeyPath(const std::string& leaf) { return kBase + leaf; }

const std::vector<std::string>& screenValueNames() {
    static const std::vector<std::string> names{"ISFULLSCREENMODE", "WIDTH",      "HEIGHT",    "BITPERPIXEL",
                                                "DEVICENAME",       "GUIDDEVICE", "GUIDDRIVER"};
    return names;
}

bool hasOwnSetup(const RegCopy& common) {
    return common.find("", "WIDTH") && common.find("", "HEIGHT") && common.find("", "DEVICENAME");
}

std::vector<RegWrite> sharedRepair(const GameRegistry& shared) {
    std::vector<RegWrite> out;
    if (!hasOwnSetup(shared.common)) return out;
    for (const std::string& name : screenValueNames())
        if (shared.personal.find("", name)) out.push_back(RegWrite{"", name, RegOp::Delete, RegValue{}});
    return out;
}

}  // namespace uagfx::settings
