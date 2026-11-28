#include "core/window_sizes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <sstream>

namespace uagfx::windowsizes {

float pulled(float from, float w0, float h0, float w, float h) {
    if (w0 <= 0 || h0 <= 0) return std::clamp(from, kLeast, kMost);
    const float share = (w / w0 + h / h0) / 2;
    return std::clamp(from * share, kLeast, kMost);
}

namespace {
Cells clamped(Cells c) {
    return Cells{std::clamp(c.columns, kLeastColumns, kMostColumns), std::clamp(c.rows, kLeastRows, kMostRows)};
}
}  // namespace

Cells cellsPulled(Cells from, float step, float dx, float dy) {
    if (step <= 0) return clamped(from);
    return clamped(Cells{from.columns + int(std::lround(dx / step)), from.rows + int(std::lround(dy / step))});
}

float wheeled(float from, int notches) { return std::clamp(from + 0.05f * float(notches), kLeast, kMost); }

Cells Sizes::cells(const std::string& window, Cells fallback) const {
    const auto it = cells_.find(window);
    return it == cells_.end() ? fallback : it->second;
}

void Sizes::setCells(const std::string& window, Cells c) { cells_[window] = clamped(c); }

int rowsPulled(int from, float step, float dy) {
    if (step <= 0) return std::clamp(from, kLeastListRows, kMostListRows);
    return std::clamp(from + int(std::lround(dy / step)), kLeastListRows, kMostListRows);
}

int Sizes::rows(const std::string& window, int fallback) const {
    const auto it = rows_.find(window);
    return it == rows_.end() ? fallback : it->second;
}

void Sizes::setRows(const std::string& window, int rows) {
    rows_[window] = std::clamp(rows, kLeastListRows, kMostListRows);
}

float Sizes::of(const std::string& window) const {
    const auto it = sizes_.find(window);
    return it == sizes_.end() ? 1.0f : it->second;
}

void Sizes::set(const std::string& window, float size) { sizes_[window] = std::clamp(size, kLeast, kMost); }

std::string Sizes::text() const {
    std::string out;
    char line[96];
    for (const auto& [name, size] : sizes_) {
        std::snprintf(line, sizeof line, "%s %.3f\n", name.c_str(), double(size));
        out += line;
    }
    for (const auto& [name, c] : cells_) {  // 3.0.24-beta.48
        std::snprintf(line, sizeof line, "%s.cells %dx%d\n", name.c_str(), c.columns, c.rows);
        out += line;
    }
    for (const auto& [name, r] : rows_) {  // 3.0.24-beta.52
        std::snprintf(line, sizeof line, "%s.rows %d\n", name.c_str(), r);
        out += line;
    }
    return out;
}

Sizes Sizes::parse(const std::string& text) {
    Sizes s;
    std::istringstream in(text);
    std::string name, value;
    while (in >> name >> value) {
        const size_t dot = name.rfind(".cells");
        if (dot != std::string::npos && dot + 6 == name.size()) {  // 3.0.24-beta.48: «items.cells 10x6»
            int columns = 0, rows = 0;
            char x = 0;
            if (std::sscanf(value.c_str(), "%d%c%d", &columns, &x, &rows) == 3 && x == 'x')
                s.setCells(name.substr(0, dot), Cells{columns, rows});
            continue;
        }
        const size_t rd = name.rfind(".rows");
        if (rd != std::string::npos && rd + 5 == name.size()) {  // 3.0.24-beta.52: «skills.rows 12»
            int rows = 0;
            if (std::sscanf(value.c_str(), "%d", &rows) == 1) s.setRows(name.substr(0, rd), rows);
            continue;
        }
        char* end = nullptr;
        const float v = std::strtof(value.c_str(), &end);
        if (end && *end == 0 && v > 0) s.set(name, v);
    }
    return s;
}

}  // namespace uagfx::windowsizes
