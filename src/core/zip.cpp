#include "core/zip.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "miniz.h"

namespace uagfx::zip {
namespace {

uint16_t get16(const uint8_t* p) { return uint16_t(p[0] | p[1] << 8); }
uint32_t get32(const uint8_t* p) { return uint32_t(get16(p)) | uint32_t(get16(p + 2)) << 16; }
uint64_t get64(const uint8_t* p) { return uint64_t(get32(p)) | uint64_t(get32(p + 4)) << 32; }

bool seek(std::FILE* f, uint64_t at) {
#ifdef _WIN32
    return _fseeki64(f, int64_t(at), SEEK_SET) == 0;
#else
    return fseeko(f, off_t(at), SEEK_SET) == 0;
#endif
}

uint64_t fileSize(std::FILE* f) {
#ifdef _WIN32
    if (_fseeki64(f, 0, SEEK_END) != 0) return 0;
    return uint64_t(_ftelli64(f));
#else
    if (fseeko(f, 0, SEEK_END) != 0) return 0;
    return uint64_t(ftello(f));
#endif
}

bool readAt(std::FILE* f, uint64_t at, void* data, size_t n) { return seek(f, at) && std::fread(data, 1, n, f) == n; }

struct File {
    std::FILE* f = nullptr;
    ~File() {
        if (f) std::fclose(f);
    }
};

}  // namespace

std::vector<std::string> Index::names() const {
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (const auto& e : entries_) out.push_back(e.first);
    return out;
}

std::string normalName(const std::string& name) {
    std::string out = name;
    for (char& c : out) c = c == '/' ? '\\' : c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return out;
}

bool Index::open(const std::string& path, const std::function<bool(const std::string&)>& keep, std::string& error) {
    entries_.clear();
    path_ = path;
    File file;
    file.f = std::fopen(path.c_str(), "rb");
    if (!file.f) {
        error = "cannot open " + path;
        return false;
    }
    const uint64_t size = fileSize(file.f);
    // The end of the central directory: in the last 64 KB and 22 bytes (its comment at most 65535).
    const uint64_t tail = std::min<uint64_t>(size, 65557);
    std::vector<uint8_t> t(size_t(tail), 0);
    if (tail < 22 || !readAt(file.f, size - tail, t.data(), t.size())) {
        error = path + " is not a zip";
        return false;
    }
    size_t e = t.size() - 22 + 1;
    while (e-- > 0)
        if (get32(&t[e]) == 0x06054b50) break;
    if (e >= t.size()) {
        error = path + " is not a zip";
        return false;
    }
    uint64_t count = get16(&t[e + 10]), dirSize = get32(&t[e + 12]), dirAt = get32(&t[e + 16]);
    if ((count == 0xFFFF || dirSize == 0xFFFFFFFF || dirAt == 0xFFFFFFFF) && e >= 20 &&
        get32(&t[e - 20]) == 0x07064b50) {  // zip64: its locator before the end record, the record where it says
        uint8_t z[56];
        if (!readAt(file.f, get64(&t[e - 20 + 8]), z, sizeof z) || get32(z) != 0x06064b50) {
            error = path + ": a damaged zip64 record";
            return false;
        }
        count = get64(z + 32);
        dirSize = get64(z + 40);
        dirAt = get64(z + 48);
    }
    if (dirAt + dirSize > size) {
        error = path + ": a damaged central directory";
        return false;
    }
    // The directory in pieces of 4 MB: an archive of 200 000 files has a directory of 20 MB, and the game's address
    // space is 32-bit.
    std::vector<uint8_t> buf;
    size_t p = 0;
    uint64_t next = dirAt;
    const uint64_t end = dirAt + dirSize;
    const auto have = [&](size_t n) {  // at least n bytes from p on
        if (buf.size() - p >= n) return true;
        buf.erase(buf.begin(), buf.begin() + std::ptrdiff_t(p));
        p = 0;
        const size_t want = std::max(n, size_t(4) << 20) - buf.size();
        const size_t take = size_t(std::min<uint64_t>(want, end - next));
        const size_t old = buf.size();
        buf.resize(old + take);
        if (take && !readAt(file.f, next, buf.data() + old, take)) {
            buf.resize(old);
            return false;
        }
        next += take;
        return buf.size() >= n;
    };
    for (uint64_t i = 0; i < count && have(46); ++i) {
        const uint8_t* h = &buf[p];
        if (get32(h) != 0x02014b50) break;
        const uint16_t nameLen = get16(h + 28), extraLen = get16(h + 30), commentLen = get16(h + 32);
        const size_t whole = 46 + size_t(nameLen) + extraLen + commentLen;
        if (!have(whole)) break;
        h = &buf[p];
        Entry en;
        en.method = get16(h + 10);
        en.compressed = get32(h + 20);
        en.size = get32(h + 24);
        en.local = get32(h + 42);
        // zip64: the sizes and the offset that do not fit come in the extra field 1, in this order.
        for (size_t x = 46 + nameLen; x + 4 <= size_t(46) + nameLen + extraLen;) {
            const uint16_t id = get16(h + x), len = get16(h + x + 2);
            if (id == 1) {
                size_t at = x + 4;
                if (en.size == 0xFFFFFFFF && at + 8 <= x + 4 + len) en.size = get64(h + at), at += 8;
                if (en.compressed == 0xFFFFFFFF && at + 8 <= x + 4 + len) en.compressed = get64(h + at), at += 8;
                if (en.local == 0xFFFFFFFF && at + 8 <= x + 4 + len) en.local = get64(h + at);
            }
            x += 4 + len;
        }
        const std::string name = normalName(std::string(reinterpret_cast<const char*>(h + 46), nameLen));
        if (keep(name)) entries_[name] = en;
        p += whole;
    }
    return true;
}

bool Index::read(const std::string& name, std::vector<uint8_t>& out, std::string& error) const {
    out.clear();
    const auto it = entries_.find(name);
    if (it == entries_.end()) {
        error = name + " is not in " + path_;
        return false;
    }
    const Entry& en = it->second;
    if ((en.method != 0 && en.method != 8) || en.size > (uint64_t(256) << 20) ||
        en.compressed > (uint64_t(256) << 20)) {
        error = name + ": not stored or deflated, or too big";
        return false;
    }
    File file;
    file.f = std::fopen(path_.c_str(), "rb");
    uint8_t local[30];
    if (!file.f || !readAt(file.f, en.local, local, sizeof local) || get32(local) != 0x04034b50) {
        error = name + ": its local header cannot be read";
        return false;
    }
    std::vector<uint8_t> data(size_t(en.compressed), 0);
    if (!readAt(file.f, en.local + 30 + get16(local + 26) + get16(local + 28), data.data(), data.size())) {
        error = name + ": its data cannot be read";
        return false;
    }
    if (en.method == 0) {
        out = std::move(data);
        return out.size() == en.size || (error = name + ": a damaged entry", false);
    }
    out.resize(size_t(en.size));
    const size_t n = tinfl_decompress_mem_to_mem(out.data(), out.size(), data.data(), data.size(), 0);
    if (n == TINFL_DECOMPRESS_MEM_TO_MEM_FAILED || n != out.size()) {
        out.clear();
        error = name + ": its data does not inflate";
        return false;
    }
    return true;
}

}  // namespace uagfx::zip
