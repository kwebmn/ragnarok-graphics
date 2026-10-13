#pragma once
#include <string>
#include <vector>

#include "core/config.hpp"

namespace uagfx {

// What the driver does with uaro-graphics.ini changed while the game runs (stage 2 spec §3.4).
struct LiveUpdate {
    Config next;                       // the running settings with every key that applies now taken from the file
    std::vector<std::string> applied;  // "sharpen 50 -> 80", in the order of the ini
    std::vector<std::string> waiting;  // keys changed in the file that apply at the next start: "mode"
};
LiveUpdate liveUpdate(const Config& running, const Config& file);

// Whether a key ("section.key", lower case) applies while the game runs (spec §2.2); false for every other key.
bool appliesNow(const std::string& sectionKey);

}  // namespace uagfx
