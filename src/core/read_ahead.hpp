#pragma once
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

// 3.0.22-beta.2 (the owner: «когда нажимаю на нпс, оно какбуто подлагивает ... какуюто прогрузку сделать еще когда
// грузится»; a Radeon R7 M370 laptop's log: frames of 581 ms round its NPCs' talks, 225 ms of them the game reading
// its archives on its own thread - an NPC's sprite 60 ms, the talk window's buttons): the files the game read slowly
// (its disk's, not the cache's) kept by the map they were read on - read ahead off the game's thread at the next start
// and map, so the game finds them in Windows' cache. A file slow on kCommonMaps maps (the talk window's, the sounds)
// is read ahead everywhere. No Windows here (win/read_ahead).
namespace uagfx::readahead {

constexpr double kSlowMs = 3.0;    // a read this slow on the game's thread: from the disk
constexpr size_t kPerMap = 200;    // a map's files kept at most (the least seen go)
constexpr size_t kMaps = 80;       // maps kept at most (the least recent go)
constexpr size_t kAhead = 400;     // read ahead at a map at most
constexpr int kCommonMaps = 2;     // slow on this many maps: read ahead at every map

inline bool slow(double ms) { return ms >= kSlowMs; }

class Learned {
public:
    // `file` as the game names it (case aside); `stamp` grows with time (the least recent go first)
    void note(const std::string& map, const std::string& file, uint64_t stamp);
    // the map's files, the most often slow first, then the common ones (on the most maps first) - kAhead at most
    std::vector<std::string> forMap(const std::string& map) const;
    std::string text() const;  // "map\tfile\ttimes\tlast" a line
    static Learned parse(const std::string& text);
    size_t size() const;  // the files kept on every map
    size_t maps() const { return maps_.size(); }

private:
    struct Seen {
        uint32_t times = 0;
        uint64_t last = 0;
    };
    struct Map {
        std::map<std::string, Seen> files;
        uint64_t last = 0;
    };
    std::map<std::string, Map> maps_;
};

}  // namespace uagfx::readahead
