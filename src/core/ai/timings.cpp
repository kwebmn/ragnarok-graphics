#include "core/ai/timings.hpp"

#include <algorithm>
#include <cstdio>

namespace uagfx::ai {
namespace {

std::string ms(double v) {
    char b[32];
    std::snprintf(b, sizeof b, v < 10.0 ? "%.1f" : "%.0f", v);
    return b;
}

}  // namespace

void Steps::add(const std::string& name, double ms) { steps_.push_back({name, ms, {}}); }

void Steps::addPart(const std::string& name, double ms) {
    if (!steps_.empty()) steps_.back().parts.emplace_back(name, ms);
}

std::string Steps::line() const {
    std::string out;
    for (const Step& s : steps_) {
        out += (out.empty() ? "" : ", ") + s.name + " " + ms(s.ms) + " ms";
        std::string parts;
        for (const auto& p : s.parts) parts += (parts.empty() ? "" : ", ") + p.first + " " + ms(p.second);
        if (!parts.empty()) out += " (" + parts + ")";
    }
    return out;
}

double Steps::total() const {
    double t = 0.0;
    for (const Step& s : steps_) t += s.ms;
    return t;
}

void LoadTimes::add(const double (&parts)[kLoadParts], double frameMs) {
    double whole = 0.0;
    for (int i = 0; i < kLoadParts; ++i) {
        sum_[i] += parts[i];
        most_[i] = std::max(most_[i], parts[i]);
        whole += parts[i];
    }
    sum_[kLoadParts] += whole;
    most_[kLoadParts] = std::max(most_[kLoadParts], whole);
    sum_[kLoadParts + 1] += frameMs;
    most_[kLoadParts + 1] = std::max(most_[kLoadParts + 1], frameMs);
    ++count_;
}

std::string LoadTimes::line() const {
    if (!count_) return {};
    static const char* const kNames[kLoadParts + 2] = {
        "in the queue", "read", "WebP", "fitted", "until its frame", "in all", "its frame lasted"};
    std::string out = std::to_string(count_) + ":";
    for (int i = 0; i < kLoadParts + 2; ++i)
        out += std::string(i == kLoadParts + 1 ? " -" : i ? "," : "") + " " + kNames[i] + " " + ms(sum_[i] / count_) +
               " (" + ms(most_[i]) + ")";
    return out;
}

}  // namespace uagfx::ai
