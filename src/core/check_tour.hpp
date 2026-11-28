#pragma once
// 3.0.24-beta.57 (the owner 28.11: «также проверку работы интерфейсов»; docs/superpowers/plans/
// 2026-11-28-full-check-probe-tour.md): the full check's windows' tour - each window opened by the game's own keys
// as the player would, its new documents checked (faint or cut words, the box on the screen), its pad ring stepped,
// Esc pressed and its closing waited for; «here» checks the windows a scene left open (an NPC's, a shop, a trade).
// The steps here, frame by frame and without Windows; win/context_check does what each step says.
#include <cstdint>
#include <string>
#include <vector>

#include "core/json.hpp"

namespace uagfx::checktour {

struct Stop {
    const char* name;
    const char* keys;  // as the MCP's key tool takes them: "alt+e"
};
// The game's own keys (win/command_window's windowName) and the Esc menu
const std::vector<Stop>& stops();

// The names in `after` not in `before`, in `after`'s order
std::vector<std::string> appeared(const std::vector<std::string>& before, const std::vector<std::string>& after);

// A box wholly on the screen
bool inside(int x, int y, int w, int h, int screenW, int screenH);

struct Seen {
    uint64_t ms = 0;
    std::vector<std::string> docs;  // the windows' documents shown (not the screen's parts)
    int padFocus = -2;              // the top window's pad ring: -2 none, -1 none chosen yet, else its box
};

enum class Do : uint8_t { Nothing, Keys, Check, PadStep, Esc, Record, Finish };
enum class Phase : uint8_t { Begin, Opening, Checked, Padded, Closing, Ended };

struct Row {
    std::string stop, error;
    std::vector<std::string> opened;
    json::Value problems;  // the ui check's, filled by the frame at Check
    bool inside = true, padRing = false, padMoves = false, closed = false;
};
json::Value rowJson(const Row& r);

struct Job {
    std::vector<std::string> only;  // stops by name; empty: all
    bool here = false;              // the windows open now, no keys, no Esc
    size_t at = 0;
    Phase phase = Phase::Begin;
    uint64_t since = 0;
    std::vector<std::string> before;
    int focusBefore = -2;
    bool built = false, told = false;
    std::vector<const Stop*> list;
    Row row;  // the stop's now (read it when step says Record)
};

Do step(Job& j, const Seen& s, int openMs = 2000);

// {"at", "of", "stop", "phase"}
json::Value status(const Job& j);

}  // namespace uagfx::checktour
