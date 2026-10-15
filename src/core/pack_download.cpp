#include "core/pack_download.hpp"

#include <algorithm>

#include <miniz.h>

namespace uagfx::pack {
namespace {

uint32_t get16(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8; }
uint32_t get32(const uint8_t* p) { return get16(p) | get16(p + 2) << 16; }

// The bytes an entry takes at most: its local header (whose extra field is unknown until read) and its data.
constexpr uint64_t kExtraMost = 1024;

}  // namespace

std::vector<Span> downloadSpans(const std::vector<ZipEntry>& remote,
                                const std::function<bool(const std::string&)>& have,
                                uint64_t end, uint64_t gapMost, uint64_t spanMost) {
    std::vector<size_t> want;
    for (size_t i = 0; i < remote.size(); ++i) {
        const ZipEntry& e = remote[i];
        if (e.name == kIndexName || e.method != 0 || e.compressed != e.size || e.header >= end || have(e.name))
            continue;
        want.push_back(i);
    }
    std::sort(want.begin(), want.end(), [&](size_t a, size_t b) { return remote[a].header < remote[b].header; });
    std::vector<Span> out;
    for (size_t i : want) {
        const ZipEntry& e = remote[i];
        const uint64_t to = std::min(end, e.header + 30 + e.name.size() + kExtraMost + e.compressed);
        if (!out.empty() && e.header <= out.back().to + gapMost && to - out.back().from <= spanMost) {
            out.back().to = std::max(out.back().to, to);
            out.back().entries.push_back(i);
            continue;
        }
        out.push_back(Span{e.header, to, {i}});
    }
    return out;
}

bool entryData(const uint8_t* span, size_t spanSize, uint64_t spanFrom, const ZipEntry& e, const uint8_t*& data,
               std::string& error) {
    if (e.header < spanFrom || e.header - spanFrom + 30 > spanSize) {
        error = e.name + ": outside what was fetched";
        return false;
    }
    const uint8_t* h = span + (e.header - spanFrom);
    const size_t room = spanSize - size_t(e.header - spanFrom);
    if (get32(h) != 0x04034b50) {
        error = e.name + ": no local header where the directory says";
        return false;
    }
    const size_t nameLen = get16(h + 26), extraLen = get16(h + 28);
    if (30 + nameLen + extraLen + e.compressed > room) {
        error = e.name + ": cut short";
        return false;
    }
    if (std::string(reinterpret_cast<const char*>(h + 30), nameLen) != e.name) {
        error = e.name + ": another name in its local header";
        return false;
    }
    data = h + 30 + nameLen + extraLen;
    if (uint32_t(mz_crc32(MZ_CRC32_INIT, data, size_t(e.compressed))) != e.crc) {
        error = e.name + ": its CRC-32 does not match";
        return false;
    }
    return true;
}

std::string mergedIndex(const std::string& local, const std::string& remote) {
    Index out = parseIndexFile(remote);
    const Index mine = parseIndexFile(local);
    for (const auto& p : mine.pictures) out.pictures.emplace(p.first, p.second);
    for (const auto& a : mine.aliases) out.aliases.emplace(a.first, a.second);
    for (const auto& a : mine.alike) out.alike.emplace(a.first, a.second);
    return indexText(out);
}

}  // namespace uagfx::pack
