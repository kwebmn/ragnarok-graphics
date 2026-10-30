#include "win/ui_cost.hpp"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

#include "win/log.hpp"

namespace uagfx::win::uicost {
namespace {

struct Part {
    const char* name = nullptr;
    int count = 0;
    double ms = 0, most = 0, words = 0;
};
Part g_parts[24];
int g_slowTold = 0;
double g_wordsMs = 0;  // 3.0.11-beta.57: drawWords's time, all of it so far
int g_fresh = 0, g_kept = 0;  // since the last told
size_t g_keptBytes = 0;

long long ticks() {
    LARGE_INTEGER t{};
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

double perMs() {
    static const double f = [] {
        LARGE_INTEGER q{};
        QueryPerformanceFrequency(&q);
        return double(q.QuadPart) / 1000.0;
    }();
    return f;
}

}  // namespace

Paint::Paint(const char* part, int width, int height)
    : part_(part), start_(ticks()), words_(g_wordsMs), width_(width), height_(height) {}

Paint::~Paint() {
    const double ms = double(ticks() - start_) / perMs(), words = g_wordsMs - words_;
    for (Part& p : g_parts) {
        if (p.name && std::strcmp(p.name, part_) != 0) continue;
        p.name = part_;
        ++p.count;
        p.ms += ms;
        p.most = std::max(p.most, ms);
        p.words += words;
        break;
    }
    if (ms > 8.0 && g_slowTold < 20) {
        ++g_slowTold;
        LOG_INFO("interface: the %s's picture (%dx%d) painted in %.1f ms (words %.1f)", part_, width_, height_, ms,
                 words);
    }
}

Words::Words() : start_(ticks()) {}

Words::~Words() {
    g_wordsMs += double(ticks() - start_) / perMs();
    g_fresh += fresh_;
    g_kept += kept_;
    if (fresh_ || kept_) g_keptBytes = bytes_;
}

void Words::counted(int fresh, int kept, size_t keptBytes) {
    fresh_ = fresh;
    kept_ = kept;
    bytes_ = keptBytes;
}

std::string take() {
    std::string out;
    double all = 0;
    for (Part& p : g_parts) {
        if (!p.name || !p.count) continue;
        char line[160];
        std::snprintf(line, sizeof line, "%s%s %d x %.1f ms (%.1f at most, words %.1f)", out.empty() ? "" : ", ",
                      p.name, p.count, p.ms / p.count, p.most, p.words / p.count);
        out += line;
        all += p.ms;
        p.count = 0;
        p.ms = p.most = p.words = 0;
    }
    if (out.empty()) return out;
    char tail[128];
    std::snprintf(tail, sizeof tail, " - %.0f ms in all; words: %d drawn anew, %d kept (%.1f MB)", all, g_fresh, g_kept,
                  double(g_keptBytes) / 1048576.0);
    g_fresh = g_kept = 0;
    return out + tail;
}

}  // namespace uagfx::win::uicost
