#include "core/frame_pace.hpp"

#include <cstdarg>
#include <cstdio>

namespace uagfx::framepace {

namespace {

std::string format(const char* pattern, ...) __attribute__((format(printf, 1, 2)));

std::string format(const char* pattern, ...) {
    char text[512];
    va_list args;
    va_start(args, pattern);
    std::vsnprintf(text, sizeof text, pattern, args);
    va_end(args);
    return text;
}

// "3 of 104 over 25 ms, 1 over 50" / "none of 10 over 25 ms"
std::string counts(uint32_t longOnes, uint32_t veryLong, uint32_t frames) {
    if (!longOnes) return format("none of %u over %.0f ms", frames, kLongMs);
    return format("%u of %u over %.0f ms, %u over %.0f", longOnes, frames, kLongMs, veryLong, kVeryLongMs);
}

// " on average, ... in the long ones" for the draws; "" with none counted
std::string drawsLine(uint64_t draws, uint32_t frames, uint64_t longDraws, uint32_t longOnes) {
    if (!draws || !frames || !longOnes) return "";
    return format("; draws a frame %.0f on average, %.0f in the long ones", double(draws) / frames,
                  double(longDraws) / longOnes);
}

// a share in per cent, "?" with nothing known
std::string percent(double part, double whole) { return whole > 0.0 ? format("%.0f %%", part * 100.0 / whole) : "?"; }

}  // namespace

double ranOf(double spanMs, uint64_t threadCycles, uint64_t tscTicks) {
    if (!tscTicks) return -1.0;
    const double share = double(threadCycles) / double(tscTicks);
    return spanMs * (share < 1.0 ? share : 1.0);
}

void GameTally::add(const GameFrame& f) {
    ++frames_;
    const double ms = f.ms();
    if (frames_ == 1 || ms > longest_.ms()) longest_ = f;
    draws_ += f.draws;
    const int at = ms > kLongMs ? 1 : 0;
    if (f.ran >= 0.0) {
        ran_[at] += f.ran;
        made_[at] += f.made;
    }
    if (ms <= kLongMs) return;
    ++long_;
    longDraws_ += f.draws;
    if (ms > kVeryLongMs) ++veryLong_;
    if (f.made >= f.own && f.made >= f.renderer)
        ++byMade_;
    else if (f.renderer >= f.own)
        ++byRenderer_;
    else
        ++byOwn_;
    if (!f.reads) return;
    ++withReads_;
    reads_ += f.reads;
    readMs_ += f.readMs;
    if (f.slowestMs > slowestMs_) {
        slowestMs_ = f.slowestMs;
        slowest_ = f.slowest;
    }
}

std::string GameTally::line() const {
    if (!frames_) return "";
    std::string line = "frames' pace, the game's side: " + counts(long_, veryLong_, frames_);
    if (!long_) return line + format("; the longest %.1f ms", longest_.ms());
    line += format("; the longest %.1f ms - the game's own frame %.1f", longest_.ms(), longest_.made);
    if (longest_.ran >= 0.0) line += format(" (ran %.1f)", longest_.ran);
    line += format(", our present %.1f, the renderer's %.1f", longest_.own, longest_.renderer);
    if (longest_.draws) line += format("; %u draws", longest_.draws);
    if (longest_.reads)
        line += format("; %u archive reads %.1f ms, the slowest %s %.1f ms", longest_.reads, longest_.readMs,
                       longest_.slowest.c_str(), longest_.slowestMs);
    line += format("; the longest part of the long ones - the game's own frame %u, the renderer's %u, ours %u", byMade_,
                   byRenderer_, byOwn_);
    if (withReads_)
        line += format("; archive reads in %u of them (%u, %.1f ms; the slowest %s %.1f ms)", withReads_, reads_,
                       readMs_, slowest_.c_str(), slowestMs_);
    line += drawsLine(draws_, frames_, longDraws_, long_);
    if (made_[1] > 0.0)
        line += "; the game's thread ran " + percent(ran_[1], made_[1]) + " of their own frames' time (" +
                percent(ran_[0], made_[0]) + " in the others)";
    return line;
}

void DrawnTally::add(const DrawnFrame& f) {
    ++frames_;
    if (frames_ == 1 || f.ms > longest_.ms) longest_ = f;
    draws_ += f.draws;
    const int at = f.ms > kLongMs ? 1 : 0;
    if (f.ran >= 0.0 && f.drawing > f.card) {
        ran_[at] += f.ran;
        worked_[at] += f.drawing - f.card;
    }
    if (f.ms <= kLongMs) return;
    ++long_;
    longDraws_ += f.draws;
    longCard_ += f.card;
    if (f.ms > kVeryLongMs) ++veryLong_;
    const double parts[4] = {f.waiting(), f.drawing, f.pace, f.present};
    int most = 0;
    for (int i = 1; i < 4; ++i)
        if (parts[i] > parts[most]) most = i;
    ++by_[most];
    if (most != 1 || !f.part) return;
    Dearest& d = dearest_[f.part];
    ++d.times;
    if (f.partMs > d.most) d.most = f.partMs;
}

std::string DrawnTally::line() const {
    if (!frames_) return "";
    std::string line = "frames' pace, the renderer's side: " + counts(long_, veryLong_, frames_);
    if (!long_) return line + format("; the longest %.1f ms", longest_.ms);
    line += format("; the longest %.1f ms - waiting for the game's frame %.1f, its drawing %.1f", longest_.ms,
                   longest_.waiting(), longest_.drawing);
    if (longest_.ran >= 0.0)
        line += format(" (ran %.1f, the card's slot %.1f)", longest_.ran, longest_.card);
    else if (longest_.card > 0.0)
        line += format(" (the card's slot %.1f)", longest_.card);
    line += format(", the frame limit's wait %.1f, the present %.1f", longest_.pace, longest_.present);
    if (longest_.part) line += format(" (its drawing's dearest: %s %.1f)", longest_.part, longest_.partMs);
    if (longest_.draws) line += format("; %u draws", longest_.draws);
    line += format("; the longest part of the long ones - waiting for the game's frame %u, its drawing %u, the frame "
                   "limit's wait %u, the present %u",
                   by_[0], by_[1], by_[2], by_[3]);
    if (!dearest_.empty()) {
        line += "; the dearest of the drawing in them -";
        bool first = true;
        for (const auto& [name, d] : dearest_) {
            line += format("%s %s %u (%.1f ms at most)", first ? "" : ",", name.c_str(), d.times, d.most);
            first = false;
        }
    }
    line += drawsLine(draws_, frames_, longDraws_, long_);
    if (worked_[1] > 0.0)
        line += "; its drawing ran " + percent(ran_[1], worked_[1]) + " of its time in them (" +
                percent(ran_[0], worked_[0]) + format(" in the others), the card's slot %.1f ms a frame in them",
                                                      longCard_ / long_);
    return line;
}

}  // namespace uagfx::framepace
