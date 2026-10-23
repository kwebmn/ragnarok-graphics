#include "core/pe_exports.hpp"

#include <algorithm>

namespace uagfx::pe {
namespace {

template <typename T>
bool get(const Read& read, uint32_t rva, T& out) {
    return read(rva, &out, sizeof out);
}

// The optional header's place and its size, and the sections' count (IMAGE_NT_HEADERS32); false when not a PE32
bool headers(const Read& read, uint32_t& optional, uint16_t& optionalSize, uint16_t& sections) {
    uint16_t mz = 0, magic = 0;
    uint32_t at = 0, signature = 0;
    if (!get(read, 0, mz) || mz != 0x5A4D || !get(read, 0x3C, at) || !get(read, at, signature) ||
        signature != 0x4550 || !get(read, at + 6, sections) || !get(read, at + 20, optionalSize))
        return false;
    optional = at + 24;
    return get(read, optional, magic) && magic == 0x10B;
}

}  // namespace

std::vector<Export> exportsOf(const Read& read) {
    std::vector<Export> out;
    uint32_t optional = 0, count = 0, dir = 0, dirSize = 0;
    uint16_t optionalSize = 0, sections = 0;
    if (!headers(read, optional, optionalSize, sections) || !get(read, optional + 92, count) || count < 1 ||
        !get(read, optional + 96, dir) || !get(read, optional + 100, dirSize) || !dir)
        return out;
    uint32_t functions = 0, names = 0, places = 0, nameList = 0, ordinals = 0;
    if (!get(read, dir + 20, functions) || !get(read, dir + 24, names) || !get(read, dir + 28, places) ||
        !get(read, dir + 32, nameList) || !get(read, dir + 36, ordinals) || functions > 65536 || names > 65536)
        return out;
    std::vector<uint32_t> rvas(functions), nameRvas(names);
    std::vector<uint16_t> ordinal(names);
    if ((functions && !read(places, rvas.data(), functions * 4u)) ||
        (names && (!read(nameList, nameRvas.data(), names * 4u) || !read(ordinals, ordinal.data(), names * 2u))))
        return out;
    for (uint32_t i = 0; i < names; ++i) {
        if (ordinal[i] >= functions) continue;
        const uint32_t rva = rvas[ordinal[i]];
        if (!rva || (rva >= dir && rva < dir + dirSize)) continue;  // none, or a forwarder ("NTDLL.RtlXxx")
        char name[128] = {};  // at once, or (near the image's end) a character at a time
        size_t n = 0;
        if (read(nameRvas[i], name, sizeof name - 1)) {
            n = std::find(name, name + sizeof name - 1, '\0') - name;
        } else {
            while (n + 1 < sizeof name && get(read, nameRvas[i] + uint32_t(n), name[n]) && name[n]) ++n;
            name[n] = 0;
        }
        if (n) out.push_back({rva, std::string(name, n)});
    }
    std::sort(out.begin(), out.end(), [](const Export& a, const Export& b) { return a.rva < b.rva; });
    return out;
}

const Export* exportAt(const std::vector<Export>& sorted, uint32_t rva) {
    const auto it = std::upper_bound(sorted.begin(), sorted.end(), rva,
                                     [](uint32_t r, const Export& e) { return r < e.rva; });
    return it == sorted.begin() ? nullptr : &*(it - 1);
}

bool codeOf(const Read& read, uint32_t& begin, uint32_t& end) {
    uint32_t optional = 0;
    uint16_t optionalSize = 0, sections = 0;
    if (!headers(read, optional, optionalSize, sections)) return false;
    begin = end = 0;
    for (uint32_t i = 0; i < sections && i < 96; ++i) {
        const uint32_t at = optional + optionalSize + i * 40;
        uint32_t size = 0, rva = 0, flags = 0;
        if (!get(read, at + 8, size) || !get(read, at + 12, rva) || !get(read, at + 36, flags)) return false;
        if (!(flags & 0x20000000) || !size) continue;  // IMAGE_SCN_MEM_EXECUTE
        if (!end || rva < begin) begin = rva;
        end = std::max(end, rva + size);
    }
    return end > begin;
}

}  // namespace uagfx::pe
