#pragma once
#include <string>

// 3.0.4-beta.35 (the user: «у людей отключается трава на слабых видеокартах»): a card weak by its memory (Direct3D 11,
// or under 4 GB of its own - integrated graphics show a few hundred MB: a Radeon 780M 417) judged by how it draws. It
// draws the weak set with its own work at most half of a frame's time for a minute on a map: the medium set (the grass
// with it) on trial; three minutes on it without the card holding the game back (keep_fps's slowForCard, twice in a
// row): kept, written down; else back to the weak set, written down too - each card tried once.
namespace uagfx::card_speed {

constexpr int kRaiseWindows = 12;  // 5-second windows on a map in a row with room: a minute
constexpr int kTrialWindows = 36;  // on the medium set: three minutes
constexpr int kLowerWindows = 2;   // in a row with the card holding the game back

// Its own work a frame (ms; below 0 not measured) at most half of a frame's time at `target` frames a second.
bool hasRoom(int target, double gpuMs);

enum class Step { None, Raise, Keep, Lower };

class Judge {
public:
    // level: the card's for the automatic set now; decided: its verdict written already - nothing more to judge
    explicit Judge(int level, bool decided = false) : level_(level), done_(decided || level != 1) {}
    // a 5-second window on a map with the game in front: its frames a second, the target (the frame limit's), the
    // card's work a frame; slowTarget the trial's bar - keep_fps's when the player set it, else the target
    Step window(double fps, int target, double gpuMs, int slowTarget = 0);
    int level() const { return level_; }
    bool trying() const { return !done_ && level_ == 2; }

private:
    int level_;
    bool done_;
    int run_ = 0, trial_ = 0, slow_ = 0;
};

// uaro-graphics\card-level.txt: "<level> <the card's name>" - 2 the medium set kept, 1 tried and too slow.
struct Record {
    int level = 0;
    std::string adapter;
};
std::string format(const Record& r);
bool parse(const std::string& text, Record& out);
// The level the automatic set takes at the start: a weak card's written verdict when the file names it, else its
// memory's; whether there is a verdict for it (no trial then).
int levelAt(int memoryLevel, const std::string& adapter, const std::string& text);
bool decided(int memoryLevel, const std::string& adapter, const std::string& text);

}  // namespace uagfx::card_speed
