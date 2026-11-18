#include "core/read_ahead.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace uagfx::readahead {
namespace {

std::string folded(const std::string& s) {
    std::string out = s;
    for (char& c : out) c = char(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

}  // namespace

void Learned::note(const std::string& map, const std::string& file, uint64_t stamp) {
    if (file.empty()) return;
    Map& m = maps_[folded(map)];
    m.last = std::max(m.last, stamp);
    Seen& s = m.files[folded(file)];
    ++s.times;
    s.last = std::max(s.last, stamp);
    if (m.files.size() > kPerMap) {  // the least seen (then the least recent) goes
        auto worst = m.files.begin();
        for (auto it = m.files.begin(); it != m.files.end(); ++it)
            if (it->second.times < worst->second.times ||
                (it->second.times == worst->second.times && it->second.last < worst->second.last))
                worst = it;
        m.files.erase(worst);
    }
    while (maps_.size() > kMaps) {
        auto oldest = maps_.begin();
        for (auto it = maps_.begin(); it != maps_.end(); ++it)
            if (it->second.last < oldest->second.last) oldest = it;
        maps_.erase(oldest);
    }
}

std::vector<std::string> Learned::forMap(const std::string& map) const {
    std::vector<std::string> out;
    const auto own = maps_.find(folded(map));
    if (own != maps_.end()) {
        std::vector<std::pair<std::string, Seen>> files(own->second.files.begin(), own->second.files.end());
        std::stable_sort(files.begin(), files.end(), [](const auto& a, const auto& b) {
            return a.second.times != b.second.times ? a.second.times > b.second.times : a.second.last > b.second.last;
        });
        for (const auto& f : files) out.push_back(f.first);
    }
    std::map<std::string, int> onMaps;
    for (const auto& [name, m] : maps_)
        for (const auto& f : m.files) ++onMaps[f.first];
    std::vector<std::pair<std::string, int>> common;
    for (const auto& [file, n] : onMaps)
        if (n >= kCommonMaps && std::find(out.begin(), out.end(), file) == out.end()) common.emplace_back(file, n);
    std::stable_sort(common.begin(), common.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    for (const auto& c : common) out.push_back(c.first);
    if (out.size() > kAhead) out.resize(kAhead);
    return out;
}

std::string Learned::text() const {
    std::string out;
    for (const auto& [map, m] : maps_)
        for (const auto& [file, s] : m.files)
            out += map + '\t' + file + '\t' + std::to_string(s.times) + '\t' + std::to_string(s.last) + '\n';
    return out;
}

Learned Learned::parse(const std::string& text) {
    Learned l;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t a = line.find('\t'), b = a == std::string::npos ? a : line.find('\t', a + 1);
        const size_t c = b == std::string::npos ? b : line.find('\t', b + 1);
        if (c == std::string::npos || b == a + 1) continue;
        const std::string map = line.substr(0, a), file = line.substr(a + 1, b - a - 1);
        const uint32_t times = uint32_t(std::strtoul(line.c_str() + b + 1, nullptr, 10));
        const uint64_t last = std::strtoull(line.c_str() + c + 1, nullptr, 10);
        if (!times) continue;
        Map& m = l.maps_[folded(map)];
        Seen& s = m.files[folded(file)];
        s.times = times;
        s.last = last;
        m.last = std::max(m.last, last);
    }
    return l;
}

size_t Learned::size() const {
    size_t n = 0;
    for (const auto& [name, m] : maps_) n += m.files.size();
    return n;
}

}  // namespace uagfx::readahead
