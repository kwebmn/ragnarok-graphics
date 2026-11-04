#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace uagfx::mobsearch {

// 3.0.11-beta.213 (the owner: «сделай так чтобы вывод команды @mobsearch показывался на миникарте, ну и чтобы это
// можно было потом убрать с миникарты»): uAthena's @mobsearch answers in the chat (atcommand.c): «Mob Search... NAME
// MAP», then a line a monster «%2d[%3d:%3d] NAME» - its number, its cell, its name. Read into the cells the minimap
// marks (win/minimap_proto) until cleared there, another search, or another map.
struct Point {
    int x = 0, y = 0;
    std::string name;
};
struct Result {
    std::string what, map;
    std::vector<Point> points;
};
std::optional<std::pair<std::string, std::string>> header(const std::string& line);  // the name, the map
std::optional<Point> point(const std::string& line);
bool sameMap(const std::string& searched, const std::string& shown);  // as the game names it (.rsw, .gat, any case)

// The chat's lines as they come: a header starts a search, its lines follow it while each comes within 3 s of the
// one before; any other line ends the run (a later look-alike is not the search's)
class Reader {
public:
    bool line(const std::string& text, uint64_t nowMs);  // true: the search's
    bool active() const { return active_; }
    const Result& result() const { return result_; }
    void clear();

private:
    Result result_;
    bool active_ = false, collecting_ = false;
    uint64_t lastAt_ = 0;
};
constexpr uint64_t kLineGapMs = 3000;

}  // namespace uagfx::mobsearch
