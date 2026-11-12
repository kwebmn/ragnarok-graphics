#pragma once
#include <string>
#include <vector>

#include "core/config.hpp"

namespace uagfx::reportchecks {

// 3.0.18-beta.189 (R-VHMGXR, the owner: «словил глюк с берсрком» - the ground and the walls washed pink-white, the
// models as they were; R-E4HFCF and R-RH3QMJ on 9.11 the same, still there after Berserk had ended and with the
// lamps off): a report's check shots. Right after the report's own picture the driver draws the same place a few
// frames each with one part of its own off and takes each of them small - the report carries them as
// check-<name>.jpg next to shot1.jpg, and which part makes a wrong look is seen in that one report.
struct Variant {
    const char* name;                  // check-<name>.jpg; the file's name in the checks' folder
    const char* what;                  // for the log
    std::vector<std::string> off;      // effects.<switch> or effects.group_<word>, off for it
    bool pause = false;                // every effect of ours off - the game's own picture, as Pause has it
};
const std::vector<Variant>& variants();
constexpr int kHoldFrames = 8;  // each one drawn this many frames, its shot the last (DLSS's history settled by then)
constexpr int kShrink = 3;      // the shots a third of the picture a side (shot_spec's «|/3»)
constexpr int kMostShots = 6;   // the site takes six pictures a report (GraphicsReportController MAX_SHOTS)

// The effects a variant draws with: `file` the ini's own (parseConfig's, the sets and the groups not yet applied),
// `cardLevel` the card's (effect_groups::effective); a pause variant's are effectsOff()
EffectSettings effectsOf(const Variant& v, EffectSettings file, int cardLevel);

// The main shot's name without its folder and «.png» (uaro-20261112-094332)
std::string baseOf(const std::string& mainShot);
// Its check shot `name`: <folder>\<base>-<name>.png
std::string shotPath(const std::string& folder, const std::string& mainShot, const char* name);

// The frames of a report's checks, after its picture's
class Run {
public:
    void start(const std::string& folder, const std::string& mainShot);
    void stop() { index_ = -1; }
    bool active() const { return index_ >= 0; }
    // A present: the variant drawn in it (-1 none: the report's own frame, the one start() came in, or the run over)
    // and, in the last of its frames, its shot's path
    int frame(std::string& shot);

private:
    int index_ = -1, held_ = 0;
    std::string folder_, main_;
};

}  // namespace uagfx::reportchecks
