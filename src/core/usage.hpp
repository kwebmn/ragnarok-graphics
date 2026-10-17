#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/config.hpp"

namespace uagfx::usage {

// 2.2.1: what the players like - with their leave ([stats] send = 1), the driver notes how the game is played: each
// stretch of play with one setup a line (the card, the quality of effects, the groups, the effects and the AI
// pictures on, the minutes, the frames a second and the card's milliseconds), each change made in the game a line
// (what was turned off after it was tried). JSON lines in uaro-graphics\usage.jsonl; the settings app sends them to
// uaro.kiev.ua (the driver never goes to the network). Nothing of the player's: no name, no login, no path.

struct Machine {  // the same all the game long
    std::string version, card, api;  // api: "d3d12", "d3d11"
    unsigned vendor = 0;
    unsigned long long vramMb = 0;
    int level = 0;  // effect_groups::cardLevel
};

struct Setup {  // what a stretch is played with
    std::string game, output;  // "1920x1080"
    std::string dlss, preset, groups, effects;  // groups "light:1:100,..."; effects "torch,lamps,..." (on)
    int aiKinds = 0;  // aiKindsShown's bits
    std::string aiModel;
    bool operator==(const Setup& o) const {
        return game == o.game && output == o.output && dlss == o.dlss && preset == o.preset && groups == o.groups &&
               effects == o.effects && aiKinds == o.aiKinds && aiModel == o.aiModel;
    }
    bool operator!=(const Setup& o) const { return !(*this == o); }
};

constexpr double kStretchMinutes = 5.0;  // a stretch closed at least this often

class Recorder {
public:
    void begin(const Machine& m, uint64_t session);
    // Each present in front: `seconds` since the last, the map outdoors, the card's ms of the frame (< 0: none new),
    // the setup now. A stretch closed (the setup changed, or kStretchMinutes went by) is added to `out`.
    void frame(double seconds, bool outdoors, double gpuMs, const Setup& s, std::string& out);
    void change(const std::string& what, std::string& out);  // "grass 1 -> 0" (the driver's live settings' line)
    void close(std::string& out);  // the open stretch, at the end
    bool begun() const { return begun_; }

private:
    void line(std::string& out);
    Machine machine_;
    std::string session_;
    Setup setup_;
    bool begun_ = false, open_ = false;
    double minutes_ = 0.0, outdoor_ = 0.0, frames_ = 0.0, gpuSum_ = 0.0, total_ = 0.0;
    int gpuCount_ = 0;
};

std::string jsonString(const std::string& text);  // "..." with the escapes JSON needs

// 2.2.2: the changes worth a line (Recorder::change) from the live update's (`applied`, "grass 1 -> 0" in the file's
// words): the effects' switches and the mode by what the set shows (effect_groups::shown) and only while they are the
// player's own (custom) - a set made the player's own rewrites every switch while nothing drawn changes, and a set
// chosen switches many at once (its own line says so); every other line as it is.
std::vector<std::string> changes(const EffectSettings& was, const EffectSettings& now, int cardLevel,
                                 const std::vector<std::string>& applied);

// What the settings app sends (POST uaro.kiev.ua/api/graphics/stats): {"id","version","lines"} - the lines from the
// start, whole ones, at most `most` bytes of them; `taken` how many bytes of `lines` went in (0: none fit).
std::string uploadBody(const std::string& id, const std::string& version, const std::string& lines, size_t most,
                       size_t& taken);
constexpr size_t kMostUpload = 1500000;

}  // namespace uagfx::usage
