#pragma once
#include <map>
#include <string>

namespace uagfx::reflive {

// 3.0.11-beta.10 (the owner: live data, «все сразу»; the site's public /api/public/online.json, as its «Онлайн» page
// and world map have it): the players on each map now. The game's watcher asks the site every 2 minutes while the game
// runs and keeps the counts alone - no names - in uaro-graphics\reference\online.json; the driver reads that. No
// Windows here.

struct Online {
    long long at = 0;  // unix time the site answered
    int online = 0;    // the server's players, all
    std::map<std::string, int> maps;
};
constexpr long long kFreshSeconds = 600;  // older: not shown (the watcher gone, the site down)

// The site's online.json ({"online": N, "byMap": [{"code", "count"}, ...]}) asked at `now`; false when it is not one
bool fromSite(const std::string& json, long long now, Online& out);
// The file's words and back ({"v":1,"at":...,"online":...,"maps":{code: count}})
std::string write(const Online& o);
bool read(const std::string& text, Online& out);
bool fresh(const Online& o, long long now);

}  // namespace uagfx::reflive
