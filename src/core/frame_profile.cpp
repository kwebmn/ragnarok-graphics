#include "core/frame_profile.hpp"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <utility>

namespace uagfx::frame_profile {
namespace {

using Part = std::pair<std::string, double>;

// "name 1.23, name 0.45" - the dearest first, names alike in their order of the alphabet
std::string listed(std::vector<Part> parts) {
    std::stable_sort(parts.begin(), parts.end(), [](const Part& a, const Part& b) { return a.second > b.second; });
    std::string out;
    for (const Part& p : parts) {
        char ms[32];
        std::snprintf(ms, sizeof ms, " %.2f", p.second);
        out += (out.empty() ? "" : ", ") + p.first + ms;
    }
    return out.empty() ? "nothing timed" : out;
}

std::string total(double ms) {
    char text[32];
    std::snprintf(text, sizeof text, "%.2f ms", ms);
    return text;
}

}  // namespace

void Window::span(const std::string& name, double gpuMs, double cpuMs) {
    Sum& s = spans_[name];
    if (gpuMs >= 0.0) {
        s.gpu += gpuMs;
        s.timed = true;
    }
    s.cpu += cpuMs;
}

void Window::cpu(const std::string& name, double ms) { apart_[name] += ms; }

void Window::frame(double ms) { frameMs_.push_back(ms); }

std::vector<std::string> Window::lines(const std::string& who, double seconds, const std::string& tag) const {
    std::vector<std::string> out;
    const size_t n = frameMs_.size();
    if (!n) return out;
    const std::string profile = tag.empty() ? "profile: " : "profile [" + tag + "]: ";
    double sum = 0.0;
    for (double ms : frameMs_) sum += ms;
    std::vector<double> slowest = frameMs_;
    std::sort(slowest.begin(), slowest.end(), std::greater<double>());
    const size_t tenth = std::max<size_t>(1, n / 10);
    double slow = 0.0;
    for (size_t i = 0; i < tenth; ++i) slow += slowest[i];
    char head[256];
    std::snprintf(head, sizeof head,
                  "%s - %zu frames in %.1f s (%.1f a second); a frame %.1f ms on average, the slowest tenth %.1f ms",
                  who.c_str(), n, seconds, seconds > 0.0 ? double(n) / seconds : 0.0, sum / double(n),
                  slow / double(tenth));
    out.push_back(profile + head);
    std::vector<Part> card, processor;
    double cardMs = 0.0, processorMs = 0.0;
    for (const auto& [name, s] : spans_) {
        if (s.timed) {
            card.emplace_back(name, s.gpu / double(n));
            cardMs += s.gpu / double(n);
        }
        processor.emplace_back(name, s.cpu / double(n));
        processorMs += s.cpu / double(n);
    }
    out.push_back(profile + "the card " + total(cardMs) + " a frame - " + listed(card) + " (ms)");
    out.push_back(profile + "the processor " + total(processorMs) + " a frame in the spans - " + listed(processor) +
                  " (ms)");
    if (!apart_.empty()) {
        std::vector<Part> parts;
        for (const auto& [name, ms] : apart_) parts.emplace_back(name, ms / double(n));
        out.push_back(profile + "the processor's parts counted apart - " + listed(parts) + " (ms a frame)");
    }
    return out;
}

void Window::clear() {
    spans_.clear();
    apart_.clear();
    frameMs_.clear();
}

void Flush::cut(const std::string& tag, bool byCuts) {
    tag_ = tag;
    asked_ = true;
    next_ = byCuts;
}

bool Flush::due(double ms, bool& log, std::string& tag) {
    if (asked_) {
        asked_ = false;
        byCuts_ = next_;
        log = !tag_.empty();
        tag = tag_;
        return true;
    }
    if (byCuts_ || ms < kWindowMs) return false;
    log = true;
    tag.clear();
    return true;
}

void Flush::reset() {
    asked_ = byCuts_ = false;
    tag_.clear();
}

}  // namespace uagfx::frame_profile
