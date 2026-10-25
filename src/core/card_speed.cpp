#include "core/card_speed.hpp"

#include "core/effect_costs.hpp"

namespace uagfx::card_speed {

bool hasRoom(int target, double gpuMs) {
    return target > 0 && gpuMs >= 0.0 && gpuMs <= 0.5 * 1000.0 / double(target);
}

Step Judge::window(double fps, int target, double gpuMs, int slowTarget) {
    if (done_) return Step::None;
    if (level_ == 1) {
        run_ = hasRoom(target, gpuMs) ? run_ + 1 : 0;
        if (run_ < kRaiseWindows) return Step::None;
        level_ = 2;
        trial_ = slow_ = 0;
        return Step::Raise;
    }
    ++trial_;
    slow_ = effect_costs::slowForCard(fps, slowTarget > 0 ? slowTarget : target, gpuMs) ? slow_ + 1 : 0;
    if (slow_ >= kLowerWindows) {
        level_ = 1;
        done_ = true;
        return Step::Lower;
    }
    if (trial_ < kTrialWindows) return Step::None;
    done_ = true;
    return Step::Keep;
}

std::string format(const Record& r) { return std::to_string(r.level) + " " + r.adapter + "\n"; }

bool parse(const std::string& text, Record& out) {
    const size_t space = text.find(' ');
    if (space == std::string::npos || space == 0) return false;
    const std::string level = text.substr(0, space);
    if (level != "1" && level != "2") return false;
    std::string adapter = text.substr(space + 1);
    while (!adapter.empty() && (adapter.back() == '\n' || adapter.back() == '\r' || adapter.back() == ' '))
        adapter.pop_back();
    if (adapter.empty()) return false;
    out.level = level[0] - '0';
    out.adapter = adapter;
    return true;
}

bool decided(int memoryLevel, const std::string& adapter, const std::string& text) {
    Record r;
    return memoryLevel == 1 && !adapter.empty() && parse(text, r) && r.adapter == adapter;
}

int levelAt(int memoryLevel, const std::string& adapter, const std::string& text) {
    Record r;
    return decided(memoryLevel, adapter, text) && parse(text, r) ? r.level : memoryLevel;
}

}  // namespace uagfx::card_speed
