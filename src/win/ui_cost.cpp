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
    double ms = 0, most = 0;
};
Part g_parts[24];
int g_slowTold = 0;

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

Paint::Paint(const char* part, int width, int height) : part_(part), start_(ticks()), width_(width), height_(height) {}

Paint::~Paint() {
    const double ms = double(ticks() - start_) / perMs();
    for (Part& p : g_parts) {
        if (p.name && std::strcmp(p.name, part_) != 0) continue;
        p.name = part_;
        ++p.count;
        p.ms += ms;
        p.most = std::max(p.most, ms);
        break;
    }
    if (ms > 8.0 && g_slowTold < 20) {
        ++g_slowTold;
        LOG_INFO("interface: the %s's picture (%dx%d) painted in %.1f ms", part_, width_, height_, ms);
    }
}

std::string take() {
    std::string out;
    double all = 0;
    for (Part& p : g_parts) {
        if (!p.name || !p.count) continue;
        char line[128];
        std::snprintf(line, sizeof line, "%s%s %d x %.1f ms (%.1f at most)", out.empty() ? "" : ", ", p.name, p.count,
                      p.ms / p.count, p.most);
        out += line;
        all += p.ms;
        p.count = 0;
        p.ms = p.most = 0;
    }
    if (out.empty()) return out;
    char tail[48];
    std::snprintf(tail, sizeof tail, " - %.0f ms in all", all);
    return out + tail;
}

}  // namespace uagfx::win::uicost
