#include "core/hotbar_bars.hpp"

#include <cstdlib>
#include <sstream>
#include <vector>

namespace uagfx::hotbar {
namespace {

std::string hex(const std::string& s) {
    static const char* const kDigits = "0123456789abcdef";
    std::string out;
    for (const unsigned char c : s) {
        out += kDigits[c >> 4];
        out += kDigits[c & 15];
    }
    return out;
}

bool unhex(const std::string& h, std::string& out) {
    if (h.size() % 2) return false;
    out.clear();
    for (size_t i = 0; i < h.size(); i += 2) {
        int v = 0;
        for (size_t j = i; j < i + 2; ++j) {
            const char c = h[j];
            const int d = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
            if (d < 0) return false;
            v = v * 16 + d;
        }
        out += char(v);
    }
    return true;
}

}  // namespace

Bars parseBars(const std::string& text) {
    Bars out;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        std::vector<std::string> fields;
        std::istringstream cells(line);
        for (std::string f; std::getline(cells, f, '\t');) fields.push_back(f);
        std::string name;
        if (fields.size() != size_t(kBarSlots) + 1 || !unhex(fields[0], name) || name.empty()) continue;
        Bar bar;
        bool ok = true;
        for (int j = 0; j < kBarSlots && ok; ++j) {  // "<hex>,<level>" or "-,0"
            const std::string& f = fields[size_t(j) + 1];
            const size_t comma = f.find(',');
            if (comma == std::string::npos) {
                ok = false;
                break;
            }
            const std::string value = f.substr(0, comma);
            if (value != "-" && !unhex(value, bar.slots[size_t(j)])) ok = false;
            if (bar.slots[size_t(j)] == "nothing") bar.slots[size_t(j)].clear();
            bar.levels[size_t(j)] = std::atoi(f.c_str() + comma + 1);
        }
        if (ok) out[name] = bar;
    }
    return out;
}

std::string barsText(const Bars& bars) {
    std::string out;
    for (const auto& [name, bar] : bars) {
        out += hex(name);
        for (int j = 0; j < kBarSlots; ++j) {
            const std::string& v = bar.slots[size_t(j)];
            out += '\t';
            out += v.empty() || v == "nothing" ? "-" : hex(v);
            out += ',' + std::to_string(bar.levels[size_t(j)]);
        }
        out += '\n';
    }
    return out;
}

int dropUnlearned(Bar& bar, const std::set<std::string>& learned) {
    int dropped = 0;
    for (int j = 0; j < kBarSlots; ++j) {
        std::string& v = bar.slots[size_t(j)];
        if (v.size() > 2 && v[0] == '#' && v[1] == '#' && !learned.count(v.substr(2))) {
            v.clear();
            bar.levels[size_t(j)] = 0;
            ++dropped;
        }
    }
    return dropped;
}

}  // namespace uagfx::hotbar
