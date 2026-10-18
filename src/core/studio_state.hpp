#pragma once
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/json.hpp"

namespace uagfx::studio {

// 2.3, uaRO Studio: what it keeps between its runs (studio\session.json, studio\queue.json) and how it names its
// copies (G1-G5).

// Where the studio stood: its tab, the navigator's place, the filter and the order of the list, the selection, the
// inspector's texture and view, its window.
struct Session {
    std::string tab = "textures", nav, filter, sort;
    bool sortDown = false, paused = false;
    std::vector<std::string> selected;  // keys
    std::string shown;                  // the key in the inspector
    int zoom = 1;
    std::string view = "pair";          // pair, wipe, tiled, map, difference
    int x = -1, y = -1, w = 0, h = 0;   // the window; x -1: where Windows puts it
};
json::Value toJson(const Session& s);
Session sessionOf(const json::Value& v);  // what is missing keeps its default

// The jobs: one at a time each kind of work, a job writing the pack waiting while someone else writes it.
struct Job {
    int64_t id = 0;
    std::string op;  // make, keep, discard, takeout, own, material, compact, build, catalog
    json::Value args;
    enum State : uint8_t { Queued, Running, Done, Failed } state = Queued;
    int tries = 0;
    bool writesPack = false;
    std::string why;  // a failed one's
};
class Queue {
public:
    int64_t add(const std::string& op, json::Value args, bool writesPack);
    // The first queued job that may run (a pack's writer only when the pack is free); null none. Valid until the
    // next add().
    Job* next(bool packFree);
    void started(int64_t id);
    void done(int64_t id);
    void failed(int64_t id, const std::string& why);
    void engineLost(int64_t id);  // queued again once, then failed
    void interrupted(int64_t id);  // stopped by Pause: queued again, not counted as a try
    const std::vector<Job>& jobs() const { return jobs_; }
    json::Value toJson() const;  // the jobs left: a running one as queued (it runs again), finished ones not kept
    static Queue fromJson(const json::Value& v);

private:
    Job* find(int64_t id);
    std::vector<Job> jobs_;
    int64_t next_ = 1;
};

// The engine's copy of an installed uaro-render64.exe: "engine-<version>-<8 hex of its build>.exe" (versionLine:
// uaro-graphics\version.txt, "2.2.5 build 2026-10-18 12:34"; size and time the exe's).
std::string engineName(const std::string& versionLine, uint64_t size, int64_t time);
// Of the studio's build folders (or the engines' copies), with their times: those past the newest `keep`, never the
// one running.
std::vector<std::string> buildsToDrop(const std::vector<std::pair<std::string, int64_t>>& builds,
                                      const std::string& running, size_t keep = 5);

}  // namespace uagfx::studio
