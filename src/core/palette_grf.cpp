#include "core/palette_grf.hpp"

#include <algorithm>
#include <cstring>
#include <unordered_map>

#include "miniz.h"

namespace uagfx::palettes {
namespace {

const std::string kHair = "\xB8\xD3\xB8\xAE";  // 머리
const std::string kBody = "\xB8\xF6";          // 몸
const std::string kPrefix = "data\\palette\\";

std::string lower(std::string s) {
    for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return s;
}

bool endsWith(const std::string& s, const std::string& end) {
    return s.size() >= end.size() && s.compare(s.size() - end.size(), end.size(), end) == 0;
}

bool digits(const std::string& s) {
    return !s.empty() && std::all_of(s.begin(), s.end(), [](char c) { return c >= '0' && c <= '9'; });
}

std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> out;
    std::string part;
    for (char c : s) {
        if (c == '/' || c == '\\') {  // cp949 has neither as a second byte
            if (!part.empty()) out.push_back(part);
            part.clear();
        } else {
            part += c;
        }
    }
    if (!part.empty()) out.push_back(part);
    return out;
}

// "<who>_<sex>_<colour>" of a file name without ".pal": who may hold '_', the sex none; false when it is no such name.
bool parts(const std::string& stem, std::string& who, int& colour) {
    const size_t last = stem.rfind('_');
    if (last == std::string::npos || last == 0 || !digits(stem.substr(last + 1)) || stem.size() - last > 6)
        return false;
    const size_t sex = stem.rfind('_', last - 1);
    if (sex == std::string::npos || sex == 0 || sex + 1 == last) return false;
    who = stem.substr(0, sex);
    colour = std::stoi(stem.substr(last + 1));
    return true;
}

void put32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(uint8_t(v >> (8 * i)));
}

std::vector<uint8_t> deflate(const std::vector<uint8_t>& raw) {
    mz_ulong n = mz_compressBound(mz_ulong(raw.size()));
    std::vector<uint8_t> out(n);
    if (mz_compress2(out.data(), &n, raw.data(), mz_ulong(raw.size()), 9) != MZ_OK) return {};
    out.resize(n);
    return out;
}

}  // namespace

std::string clientName(const std::string& path) {
    const std::vector<std::string> p = split(path);
    for (size_t i = 0; i + 1 < p.size(); ++i) {
        if (lower(p[i]) != "palette") continue;
        std::string name = kPrefix;
        for (size_t j = i + 1; j < p.size(); ++j) name += (j > i + 1 ? "\\" : "") + p[j];
        return name;
    }
    return "";
}

std::optional<Left> verdict(const std::string& name) {
    if (name.compare(0, kPrefix.size(), kPrefix) != 0 || !endsWith(lower(name), ".pal")) return Left::NotPalette;
    const std::vector<std::string> rest = split(name.substr(kPrefix.size()));
    if (rest.size() > 2 || (rest.size() == 2 && rest[0] != kHair && rest[0] != kBody)) return Left::Unknown;
    if (rest.size() != 2) return Left::OddName;
    const std::string stem = rest[1].substr(0, rest[1].size() - 4);
    std::string who;
    int colour = 0;
    if (!parts(stem, who, colour)) return Left::OddName;
    if (rest[0] == kHair) {  // 머리<style>_<sex>_<colour>
        if (who.compare(0, kHair.size(), kHair) != 0 || !digits(who.substr(kHair.size())) ||
            who.size() - kHair.size() > 4)
            return Left::OddName;
        const int style = std::stoi(who.substr(kHair.size()));
        if (style > kMaxHairStyle) return Left::HairPast;  // 2.5.24: any colour
        return std::nullopt;
    }
    if (colour > kMaxClothColour) return Left::ClothesPast;  // <job>_<sex>_<colour>
    return std::nullopt;
}

bool Collector::wants(const std::string& path) {
    const std::string name = clientName(path);
    if (name.empty() || !endsWith(lower(name), ".pal")) {
        ++left_[size_t(Left::NotPalette)];
        return false;
    }
    if (!names_.insert(lower(name)).second) return false;  // an earlier source's
    ++found_;
    if (const std::optional<Left> why = verdict(name)) {
        ++left_[size_t(*why)];
        return false;
    }
    return true;
}

void Collector::take(const std::string& path, std::vector<uint8_t> bytes) {
    if (bytes.empty()) {
        ++left_[size_t(Left::Empty)];
        return;
    }
    palettes_.emplace_back(clientName(path), std::move(bytes));
}

size_t Collector::hair() const {
    const std::string folder = kPrefix + kHair + "\\";
    return size_t(std::count_if(palettes_.begin(), palettes_.end(), [&folder](const auto& p) {
        return p.first.compare(0, folder.size(), folder) == 0;
    }));
}

std::vector<uint8_t> Collector::grf(uint32_t& blocks) const {
    struct Block {
        uint32_t offset, compressed, aligned, size;
    };
    std::vector<const std::pair<std::string, std::vector<uint8_t>>*> order;
    for (const auto& p : palettes_) order.push_back(&p);
    std::sort(order.begin(), order.end(), [](const auto* a, const auto* b) { return a->first < b->first; });
    std::unordered_map<std::string, Block> made;  // a palette's bytes -> its data
    std::vector<uint8_t> data, table;
    for (const auto* p : order) {
        const std::string raw(p->second.begin(), p->second.end());
        auto it = made.find(raw);
        if (it == made.end()) {
            const std::vector<uint8_t> z = deflate(p->second);
            const Block b{uint32_t(data.size()), uint32_t(z.size()), uint32_t((z.size() + 7) & ~size_t(7)),
                          uint32_t(p->second.size())};
            data.insert(data.end(), z.begin(), z.end());
            data.resize(data.size() + (b.aligned - b.compressed), 0);
            it = made.emplace(raw, b).first;
        }
        const Block& b = it->second;
        table.insert(table.end(), p->first.begin(), p->first.end());
        table.push_back(0);
        put32(table, b.compressed);
        put32(table, b.aligned);
        put32(table, b.size);
        table.push_back(1);  // a file
        put32(table, b.offset);
    }
    blocks = uint32_t(made.size());
    const std::vector<uint8_t> packed = deflate(table);
    std::vector<uint8_t> out(16, 0);
    std::memcpy(out.data(), "Master of Magic", 15);
    out.resize(30, 0);  // no key: nothing encrypted
    put32(out, uint32_t(data.size()));  // the table, after the header
    put32(out, 0);
    put32(out, uint32_t(order.size()) + 7);
    put32(out, 0x200);
    out.insert(out.end(), data.begin(), data.end());
    put32(out, uint32_t(packed.size()));
    put32(out, uint32_t(table.size()));
    out.insert(out.end(), packed.begin(), packed.end());
    return out;
}

}  // namespace uagfx::palettes
