#include "core/sample_profile.hpp"

#include <algorithm>
#include <cstdio>

namespace uagfx::profile {
namespace {

std::string percent(size_t n, size_t total) {
    char b[32];
    std::snprintf(b, sizeof b, "%.1f%%", total ? 100.0 * double(n) / double(total) : 0.0);
    return b;
}

template <typename K>
std::vector<std::pair<K, size_t>> sorted(const std::map<K, size_t>& m) {
    std::vector<std::pair<K, size_t>> v(m.begin(), m.end());
    std::stable_sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    return v;
}

// 3.0.2-beta.12: uaRO.exe's functions of each step (sample_profile.hpp), [begin, end) by the decompilation's sizes
struct StepSpan {
    uint32_t begin, end;
    Step step;
};
constexpr StepSpan kStepSpans[] = {
    {0x50D120, 0x50D1E0, Step::Files},      {0x510F20, 0x510FD0, Step::Files},     {0x510FD0, 0x511120, Step::Files},
    {0x42B030, 0x42C1A0, Step::Pictures},   {0x49D830, 0x49D950, Step::Pictures},  {0x4385F0, 0x4390C0, Step::Sprites},
    {0x5131F0, 0x513570, Step::Resources},  {0x4EA9A0, 0x4EB1D0, Step::World},     {0x559030, 0x559750, Step::MapStart},
    {0x40FDB0, 0x40FE40, Step::Present},    {0x404BC0, 0x404CB0, Step::Present},   {0x579900, 0x57D420, Step::Packets},
    {0x654100, 0x654190, Step::Messages},   {0x559750, 0x559B00, Step::Frames},    {0x42A9A0, 0x42AB00, Step::Pick},
    {0x564A20, 0x564B80, Step::Pick},
};

int stepAt(uint32_t address) {
    for (const StepSpan& s : kStepSpans)
        if (address >= s.begin && address < s.end) return int(s.step);
    return -1;
}

}  // namespace

int stepOf(uint32_t at, const uint32_t* returns, size_t count) {
    int step = stepAt(at);
    for (size_t i = 0; step < 0 && i < count; ++i) step = stepAt(returns[i]);
    return step;
}

const char* stepName(int step) {
    static const char* const kNames[int(Step::Count)] = {"reading the game's files",
                                                         "the textures' pictures",
                                                         "sprites",
                                                         "the resources made of their files",
                                                         "the world and its near models",
                                                         "the rest of the map's start",
                                                         "showing a frame",
                                                         "the server's packets",
                                                         "Windows' messages",
                                                         "the game's frames",
                                                         "the cell under the mouse"};
    return step >= 0 && step < int(Step::Count) ? kNames[step] : "elsewhere";
}

void Tally::setModules(std::vector<Module> modules, size_t exe, size_t self) {
    modules_ = std::move(modules);
    exe_ = exe;
    self_ = self;
}

int Tally::moduleOf(uint32_t at) const {
    for (size_t i = 0; i < modules_.size(); ++i)
        if (at - modules_[i].base < modules_[i].size) return int(i);
    return -1;
}

void Tally::add(uint32_t at, uint32_t caller) {
    Sample s;
    s.at = at;
    s.caller = caller;
    add(s);
}

void Tally::add(const Sample& s) {
    ++samples_;
    const int m = moduleOf(s.at);
    ++byWhere_[!s.where.empty() ? s.where : m >= 0 ? modules_[size_t(m)].name : std::string("(no module)")];
    if (m >= 0 && size_t(m) == self_) ++inSelf_[(s.at - modules_[size_t(m)].base) & ~63u];
    if (s.own && self_ < modules_.size() && s.own - modules_[self_].base < modules_[self_].size) {  // 3.0.6-beta.3
        const uint32_t rva = s.own - modules_[self_].base;
        const pe::Function* f = pe::functionAt(ownFunctions_, rva);
        ++ownTotals_[f ? f->rva : rva & ~63u];
    }
    if (m >= 0 && size_t(m) == exe_) {
        ++inExe_[s.at & ~63u];
    } else if (s.caller) {
        ++callers_[{s.caller & ~15u, !s.where.empty() ? s.where : m >= 0 ? modules_[size_t(m)].name : "?"}];
    }
    ++bySteps_[s.step];
    if (s.step >= 0) ++stepped_;
}

std::vector<std::string> Tally::report(double seconds, size_t top) {
    char head[64];
    std::snprintf(head, sizeof head, "%zu samples in %.0f s", samples_, seconds);
    return report("profile", head, top);
}

std::vector<std::string> Tally::report(const std::string& prefix, const std::string& head, size_t top) {
    std::vector<std::string> lines;
    if (!samples_) return lines;
    std::string l = prefix + ": " + head + " -";
    size_t k = 0;
    for (const auto& [where, n] : sorted(byWhere_)) {
        if (k++ == 10) break;
        l += std::string(k > 1 ? "," : "") + " " + where + " " + percent(n, samples_);
    }
    lines.push_back(l);
    if (stepped_) {  // 3.0.2-beta.12: the steps by their shares, what is under none of them last
        l = prefix + ": by the game's steps -";
        k = 0;
        for (const auto& [step, n] : sorted(bySteps_))
            if (step >= 0) l += std::string(k++ ? "," : "") + " " + stepName(step) + " " + percent(n, samples_);
        if (samples_ > stepped_) l += ", " + std::string(stepName(-1)) + " " + percent(samples_ - stepped_, samples_);
        lines.push_back(l);
    }
    if (!inExe_.empty()) {
        l = prefix + ": uaRO.exe's busiest places -";
        k = 0;
        for (const auto& [span, n] : sorted(inExe_)) {
            if (k++ == top) break;
            char b[48];
            std::snprintf(b, sizeof b, "%s 0x%06x %s", k > 1 ? "," : "", unsigned(span), percent(n, samples_).c_str());
            l += b;
        }
        lines.push_back(l);
    }
    if (!callers_.empty()) {
        l = prefix + ": called from uaRO.exe -";
        k = 0;
        for (const auto& [key, n] : sorted(callers_)) {
            if (k++ == top) break;
            char b[32];
            std::snprintf(b, sizeof b, "%s 0x%06x into ", k > 1 ? "," : "", unsigned(key.first));
            l += b + key.second + " " + percent(n, samples_);
        }
        lines.push_back(l);
    }
    if (!inSelf_.empty() && self_ < modules_.size()) {
        l = prefix + ": " + modules_[self_].name + "'s busiest places -";
        k = 0;
        for (const auto& [span, n] : sorted(inSelf_)) {
            if (k++ == top) break;
            char b[48];
            std::snprintf(b, sizeof b, "%s +0x%06x %s", k > 1 ? "," : "", unsigned(span), percent(n, samples_).c_str());
            l += b;
        }
        lines.push_back(l);
    }
    if (!ownTotals_.empty() && self_ < modules_.size()) {  // 3.0.6-beta.3
        l = prefix + ": " + modules_[self_].name + "'s functions with what they call -";
        k = 0;
        for (const auto& [start, n] : sorted(ownTotals_)) {
            if (k++ == top + 10) break;
            char b[48];
            const std::string share = percent(n, samples_);
            std::snprintf(b, sizeof b, "%s +0x%06x %s", k > 1 ? "," : "", unsigned(start), share.c_str());
            l += b;
        }
        lines.push_back(l);
    }
    samples_ = 0;
    stepped_ = 0;
    byWhere_.clear();
    inExe_.clear();
    callers_.clear();
    inSelf_.clear();
    ownTotals_.clear();
    bySteps_.clear();
    return lines;
}

}  // namespace uagfx::profile
