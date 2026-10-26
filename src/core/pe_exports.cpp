#include "core/pe_exports.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <utility>

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

namespace {

uint32_t uleb(const std::vector<uint8_t>& d, size_t& at) {
    uint32_t v = 0;
    for (int shift = 0; at < d.size() && shift < 35; shift += 7) {
        const uint8_t b = d[at++];
        v |= uint32_t(b & 0x7f) << shift;
        if (!(b & 0x80)) break;
    }
    return v;
}

// A value of a DWARF pointer encoding's size (its low four bits) at `at`; false for one not known or past the end
bool encoded(const std::vector<uint8_t>& d, size_t& at, uint8_t encoding, int64_t& out) {
    const auto take = [&](size_t n, bool sign) {
        if (at + n > d.size()) return false;
        uint64_t v = 0;
        for (size_t i = 0; i < n; ++i) v |= uint64_t(d[at + i]) << (8 * i);
        if (sign && n < 8 && (v >> (8 * n - 1) & 1)) v |= ~uint64_t(0) << (8 * n);
        out = int64_t(v);
        at += n;
        return true;
    };
    switch (encoding & 0x0f) {
    case 0x00: case 0x03: return take(4, false);  // absptr (32-bit), udata4
    case 0x0b: return take(4, true);              // sdata4
    case 0x02: return take(2, false);
    case 0x0a: return take(2, true);
    case 0x04: case 0x0c: return take(8, encoding == 0x0c);
    case 0x01: out = int64_t(uleb(d, at)); return at <= d.size();
    default: return false;
    }
}

}  // namespace

std::vector<Function> functionsOf(const Read& read) {
    std::vector<Function> out;
    uint32_t optional = 0, imageBase = 0;
    uint16_t optionalSize = 0, sections = 0;
    if (!headers(read, optional, optionalSize, sections) || !get(read, optional + 28, imageBase)) return out;
    uint32_t rva = 0, size = 0;
    for (uint32_t i = 0; i < sections && i < 96 && !size; ++i) {
        const uint32_t at = optional + optionalSize + i * 40;
        char name[8] = {};
        if (!read(at, name, 8)) return out;
        if (std::memcmp(name, ".eh_fram", 8) == 0 && get(read, at + 8, size) && get(read, at + 12, rva)) break;
        size = 0;
    }
    if (!size || size > (64u << 20)) return out;
    std::vector<uint8_t> d(size);
    if (!read(rva, d.data(), size)) return out;
    std::vector<std::pair<size_t, uint8_t>> cies;  // each CIE's place and its FDEs' place encoding
    size_t at = 0;
    while (at + 8 <= d.size()) {
        uint32_t length = 0, id = 0;
        std::memcpy(&length, &d[at], 4);
        if (length == 0 || length == 0xffffffffu || at + 4 + length > d.size()) break;
        std::memcpy(&id, &d[at + 4], 4);
        const size_t next = at + 4 + length;
        if (id == 0) {  // a CIE: its version, augmentation, alignments, return column, augmentation data
            size_t p = at + 8;
            const uint8_t version = p < next ? d[p++] : 0;
            std::string aug;
            while (p < next && d[p]) aug += char(d[p++]);
            ++p;
            uleb(d, p);  // the code's alignment
            uleb(d, p);  // the data's (signed: only skipped)
            if (version == 1) ++p; else uleb(d, p);
            uint8_t fde = 0;  // absptr
            if (!aug.empty() && aug[0] == 'z') {
                uleb(d, p);
                for (size_t k = 1; k < aug.size() && p < next; ++k) {
                    int64_t skip = 0;
                    if (aug[k] == 'P') {
                        const uint8_t enc = d[p++];
                        if (!encoded(d, p, enc, skip)) break;
                    } else if (aug[k] == 'L') {
                        ++p;
                    } else if (aug[k] == 'R') {
                        fde = d[p++];
                    }
                }
            }
            cies.emplace_back(at, fde);
        } else {  // an FDE: its CIE back from the pointer's own place, its function's place and size
            const size_t cie = at + 4 >= id ? at + 4 - id : SIZE_MAX;
            const auto c = std::find_if(cies.begin(), cies.end(), [&](const auto& e) { return e.first == cie; });
            size_t p = at + 8;
            int64_t begin = 0, length2 = 0;
            if (c != cies.end() && encoded(d, p, c->second, begin) && encoded(d, p, c->second & 0x0f, length2)) {
                int64_t place = (c->second & 0x70) == 0x10 ? int64_t(rva) + int64_t(at + 8) + begin  // pcrel
                                                           : begin - int64_t(imageBase);              // absptr
                if (place > 0 && length2 > 0 && place + length2 <= int64_t(UINT32_MAX))
                    out.push_back({uint32_t(place), uint32_t(length2)});
            }
        }
        at = next;
    }
    std::sort(out.begin(), out.end(), [](const Function& a, const Function& b) { return a.rva < b.rva; });
    return out;
}

const Function* functionAt(const std::vector<Function>& sorted, uint32_t rva) {
    const auto it = std::upper_bound(sorted.begin(), sorted.end(), rva,
                                     [](uint32_t r, const Function& f) { return r < f.rva; });
    if (it == sorted.begin()) return nullptr;
    const Function& f = *(it - 1);
    return rva - f.rva < f.size ? &f : nullptr;
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
