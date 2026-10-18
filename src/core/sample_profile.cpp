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

}  // namespace

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
    ++samples_;
    const int m = moduleOf(at);
    ++byModule_[m];
    if (m >= 0 && size_t(m) == self_) ++inSelf_[(at - modules_[size_t(m)].base) & ~63u];
    if (m >= 0 && size_t(m) == exe_) {
        ++inExe_[at & ~63u];
    } else if (caller) {
        ++callers_[uint64_t(caller & ~15u) << 32 | uint32_t(m + 1)];
    }
}

std::vector<std::string> Tally::report(double seconds, size_t top) {
    std::vector<std::string> lines;
    if (!samples_) return lines;
    char head[96];
    std::snprintf(head, sizeof head, "profile: %zu samples in %.0f s -", samples_, seconds);
    std::string l = head;
    size_t k = 0;
    for (const auto& [m, n] : sorted(byModule_)) {
        if (k++ == 10) break;
        l += std::string(k > 1 ? "," : "") + " " + (m >= 0 ? modules_[size_t(m)].name : std::string("(no module)")) +
             " " + percent(n, samples_);
    }
    lines.push_back(l);
    if (!inExe_.empty()) {
        l = "profile: uaRO.exe's busiest places -";
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
        l = "profile: called from uaRO.exe -";
        k = 0;
        for (const auto& [key, n] : sorted(callers_)) {
            if (k++ == top) break;
            const int m = int(uint32_t(key)) - 1;
            char b[96];
            std::snprintf(b, sizeof b, "%s 0x%06x into %s %s", k > 1 ? "," : "", unsigned(key >> 32),
                          m >= 0 ? modules_[size_t(m)].name.c_str() : "?", percent(n, samples_).c_str());
            l += b;
        }
        lines.push_back(l);
    }
    if (!inSelf_.empty() && self_ < modules_.size()) {
        l = "profile: " + modules_[self_].name + "'s busiest places -";
        k = 0;
        for (const auto& [span, n] : sorted(inSelf_)) {
            if (k++ == top) break;
            char b[48];
            std::snprintf(b, sizeof b, "%s +0x%06x %s", k > 1 ? "," : "", unsigned(span), percent(n, samples_).c_str());
            l += b;
        }
        lines.push_back(l);
    }
    samples_ = 0;
    byModule_.clear();
    inExe_.clear();
    callers_.clear();
    inSelf_.clear();
    return lines;
}

}  // namespace uagfx::profile
