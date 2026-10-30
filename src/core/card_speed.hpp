#pragma once
#include <string>

// 3.0.4-beta.35 (the user: «у людей отключается трава на слабых видеокартах»): a card weak by its memory (Direct3D 11,
// or under 4 GB of its own - integrated graphics show a few hundred MB: a Radeon 780M 417) judged by how it draws.
// 3.0.11-beta.58 (the autopilot, the owner: «делай»; docs/superpowers/specs/2026-10-30-autopilot-design.md): every
// card's automatic set walks the whole ladder - 0 «Без ефектів», 1 «Низькі», 2 «Середні», 3 «Високі» - to the highest
// one it holds at the frame limit (60): a minute on the maps with the game at its frames and the card's own work at
// most half a frame - the next set on trial; three minutes on it without the card holding the game back - kept, else
// back (and not tried again); a card not judged yet that keeps holding the game back half a minute - a set lower. The
// verdict is kept per card for the next games and the settings app.
namespace uagfx::card_speed {

constexpr int kRaiseWindows = 12;  // 5-second windows on a map in a row with room: a minute
constexpr int kTrialWindows = 36;  // on the set tried: three minutes
constexpr int kBackWindows = 2;    // a trial's windows in a row with the card holding the game back
constexpr int kLowerWindows = 6;   // 3.0.11-beta.58: the same at a set not judged yet - half a minute
constexpr int kTopLevel = 3;       // «Високі»

// Its own work a frame (ms; below 0 not measured) at most half of a frame's time at `target` frames a second.
bool hasRoom(int target, double gpuMs);

enum class Step { None, Raise, Keep, Back, Lower };

class Judge {
public:
    // level: the automatic set's now; settled: its verdict known already - nothing more to judge
    explicit Judge(int level, bool settled = false) : level_(level), settled_(settled) {}
    // a 5-second window on a map with the game in front: its frames a second, the target (the frame limit's), the
    // card's work a frame; slowTarget the bar of the card holding the game back - keep_fps's when set, else the target
    Step window(double fps, int target, double gpuMs, int slowTarget = 0);
    int level() const { return level_; }
    bool trying() const { return trying_; }
    bool settled() const { return settled_; }

private:
    int level_;
    bool settled_;
    bool trying_ = false;
    int room_ = 0, trial_ = 0, slow_ = 0;
};

// uaro-graphics\card-level.txt: "v2 <level> <settled 0|1> <the card's name>"; 3.0.4-beta.35's "<1|2> <the card's
// name>" read as its level, not settled.
struct Record {
    int level = 0;
    bool settled = false;
    std::string adapter;
};
std::string format(const Record& r);
bool parse(const std::string& text, Record& out);
// The automatic set's level at the start: the record's when it names this card, else the card's memory's - not judged.
struct Start {
    int level = 0;
    bool settled = false;
};
Start startOf(int memoryLevel, const std::string& adapter, const std::string& text);

}  // namespace uagfx::card_speed
