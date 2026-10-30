#include "core/card_speed.hpp"

#include "core/effect_costs.hpp"

namespace uagfx::card_speed {

bool hasRoom(int target, double gpuMs) {
    return target > 0 && gpuMs >= 0.0 && gpuMs <= 0.5 * 1000.0 / double(target);
}

Step Judge::window(double fps, int target, double gpuMs, int slowTarget) {
    if (settled_) return Step::None;
    const bool slow = effect_costs::slowForCard(fps, slowTarget > 0 ? slowTarget : target, gpuMs);
    if (trying_) {
        ++trial_;
        slow_ = slow ? slow_ + 1 : 0;
        if (slow_ >= kBackWindows) {
            --level_;
            trying_ = false;
            settled_ = true;
            return Step::Back;
        }
        if (trial_ < kTrialWindows) return Step::None;
        trying_ = false;
        settled_ = level_ >= kTopLevel;
        room_ = slow_ = 0;
        return Step::Keep;
    }
    slow_ = slow ? slow_ + 1 : 0;
    if (slow_ >= kLowerWindows && level_ > 0) {
        --level_;
        settled_ = true;
        return Step::Lower;
    }
    // room: the game at its frames (a town slow for the processor is no room - more effects add the driver's own
    // processor work there) and the card half idle
    room_ = fps >= 0.9 * double(target) && hasRoom(target, gpuMs) ? room_ + 1 : 0;
    if (room_ < kRaiseWindows || level_ >= kTopLevel) return Step::None;
    ++level_;
    trying_ = true;
    room_ = trial_ = slow_ = 0;
    return Step::Raise;
}

std::string format(const Record& r) {
    return "v2 " + std::to_string(r.level) + (r.settled ? " 1 " : " 0 ") + r.adapter + "\n";
}

namespace {

std::string trimmed(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

}  // namespace

bool parse(const std::string& text, Record& out) {
    const std::string line = trimmed(text.substr(0, text.find('\n')));
    if (line.size() > 7 && line.compare(0, 3, "v2 ") == 0 && line[4] == ' ' && line[6] == ' ') {
        const char level = line[3], settled = line[5];
        if (level < '0' || level > '3' || (settled != '0' && settled != '1')) return false;
        out = Record{level - '0', settled == '1', line.substr(7)};
        return !out.adapter.empty();
    }
    const size_t space = line.find(' ');  // 3.0.4-beta.35's
    if (space != 1 || (line[0] != '1' && line[0] != '2') || line.size() < 3) return false;
    out = Record{line[0] - '0', false, line.substr(2)};
    return true;
}

Start startOf(int memoryLevel, const std::string& adapter, const std::string& text) {
    Record r;
    if (!adapter.empty() && parse(text, r) && r.adapter == adapter) return Start{r.level, r.settled};
    return Start{memoryLevel, false};
}

}  // namespace uagfx::card_speed
