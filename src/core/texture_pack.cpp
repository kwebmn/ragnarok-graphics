#include "core/texture_pack.hpp"

#include <miniz.h>

#include <algorithm>
#include <cstring>

#if defined(_WIN32)
#include <io.h>
#define UAGFX_SEEK _fseeki64
#define UAGFX_TELL _ftelli64
#else
#include <unistd.h>
#define UAGFX_SEEK fseeko
#define UAGFX_TELL ftello
#endif

namespace uagfx::pack {
namespace {

constexpr uint32_t kLocalSig = 0x04034b50, kCentralSig = 0x02014b50, kEndSig = 0x06054b50;
constexpr uint32_t kEnd64Sig = 0x06064b50, kLocator64Sig = 0x07064b50;
constexpr uint32_t kDosDate = 0x21;  // 1980-01-01: the pictures have no time of their own

void put16(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(uint8_t(v));
    b.push_back(uint8_t(v >> 8));
}
void put32(std::vector<uint8_t>& b, uint32_t v) {
    put16(b, v & 0xFFFF);
    put16(b, v >> 16);
}
void put64(std::vector<uint8_t>& b, uint64_t v) {
    put32(b, uint32_t(v));
    put32(b, uint32_t(v >> 32));
}
uint32_t get16(const uint8_t* p) { return uint32_t(p[0]) | uint32_t(p[1]) << 8; }
uint32_t get32(const uint8_t* p) { return get16(p) | get16(p + 2) << 16; }
uint64_t get64(const uint8_t* p) { return uint64_t(get32(p)) | uint64_t(get32(p + 4)) << 32; }

bool seekTo(std::FILE* f, uint64_t at) { return UAGFX_SEEK(f, static_cast<int64_t>(at), SEEK_SET) == 0; }
bool readAt(std::FILE* f, uint64_t at, void* out, size_t n) {
    return seekTo(f, at) && std::fread(out, 1, n, f) == n;
}
uint64_t sizeOf(std::FILE* f) {
    if (UAGFX_SEEK(f, 0, SEEK_END) != 0) return 0;
    const int64_t n = UAGFX_TELL(f);
    return n < 0 ? 0 : uint64_t(n);
}
bool truncateTo(std::FILE* f, uint64_t size) {
    std::fflush(f);
#if defined(_WIN32)
    return _chsize_s(_fileno(f), static_cast<__int64>(size)) == 0;
#else
    return ftruncate(fileno(f), static_cast<off_t>(size)) == 0;
#endif
}

using Found = ZipEntry;

// The central directory of a ZIP, ZIP64 too; false when the file is no whole ZIP. 0.53: read through `read` (a file
// here, HTTP ranges of a pack on the site in the settings app's download).
}  // namespace

bool centralDirectoryOf(uint64_t size, const ReadAt& read, std::vector<Found>& out, std::string& error) {
    if (size < 22) {
        error = "too short for a ZIP";
        return false;
    }
    const uint64_t tail = std::min<uint64_t>(size, 22 + 65535);
    std::vector<uint8_t> b(static_cast<size_t>(tail));
    if (!read(size - tail, b.data(), b.size())) {
        error = "cannot be read";
        return false;
    }
    size_t at = b.size() - 22 + 1;
    bool found = false;
    while (at-- > 0)
        if (get32(&b[at]) == kEndSig) {
            found = true;
            break;
        }
    if (!found) {
        error = "no central directory";
        return false;
    }
    const uint64_t endAt = size - tail + at;
    uint64_t count = get16(&b[at + 10]), cdSize = get32(&b[at + 12]), cdOffset = get32(&b[at + 16]);
    if (count == 0xFFFF || cdSize == 0xFFFFFFFF || cdOffset == 0xFFFFFFFF) {
        uint8_t loc[20], end64[56];
        if (endAt < 20 || !read(endAt - 20, loc, sizeof loc) || get32(loc) != kLocator64Sig ||
            !read(get64(loc + 8), end64, sizeof end64) || get32(end64) != kEnd64Sig) {
            error = "no ZIP64 end record";
            return false;
        }
        count = get64(end64 + 32);
        cdSize = get64(end64 + 40);
        cdOffset = get64(end64 + 48);
    }
    if (cdOffset > size || cdSize > size - cdOffset || cdSize > (uint64_t(1) << 31)) {
        error = "a broken central directory";
        return false;
    }
    std::vector<uint8_t> cd(static_cast<size_t>(cdSize));
    if (!cd.empty() && !read(cdOffset, cd.data(), cd.size())) {
        error = "the central directory cannot be read";
        return false;
    }
    out.clear();
    size_t p = 0;
    for (uint64_t i = 0; i < count; ++i) {
        if (p + 46 > cd.size() || get32(&cd[p]) != kCentralSig) {
            error = "a broken central directory entry";
            return false;
        }
        Found e;
        e.method = get16(&cd[p + 10]);
        e.crc = get32(&cd[p + 16]);
        e.compressed = get32(&cd[p + 20]);
        e.size = get32(&cd[p + 24]);
        const size_t nameLen = get16(&cd[p + 28]), extraLen = get16(&cd[p + 30]), commentLen = get16(&cd[p + 32]);
        e.header = get32(&cd[p + 42]);
        const size_t extraEnd = p + 46 + nameLen + extraLen;
        if (extraEnd + commentLen > cd.size()) {
            error = "a broken central directory entry";
            return false;
        }
        e.name.assign(reinterpret_cast<const char*>(&cd[p + 46]), nameLen);
        // ZIP64: the eight-byte values of the fields that are all ones, in this order.
        for (size_t x = p + 46 + nameLen; x + 4 <= extraEnd;) {
            const size_t id = get16(&cd[x]), len = get16(&cd[x + 2]);
            if (x + 4 + len > extraEnd) break;
            if (id == 1) {
                size_t q = x + 4;
                const size_t qEnd = x + 4 + len;
                if (e.size == 0xFFFFFFFF && q + 8 <= qEnd) e.size = get64(&cd[q]), q += 8;
                if (e.compressed == 0xFFFFFFFF && q + 8 <= qEnd) e.compressed = get64(&cd[q]), q += 8;
                if (e.header == 0xFFFFFFFF && q + 8 <= qEnd) e.header = get64(&cd[q]);
            }
            x += 4 + len;
        }
        out.push_back(std::move(e));
        p = extraEnd + commentLen;
    }
    return true;
}

namespace {

bool centralDirectory(std::FILE* f, std::vector<Found>& out, std::string& error) {
    return centralDirectoryOf(sizeOf(f), [f](uint64_t at, uint8_t* to, size_t n) { return readAt(f, at, to, n); },
                              out, error);

}

// A pack whose last run stopped before its central directory: its pictures one after another from the start, each
// with its sizes in its local header - up to the first that is not whole.
void localHeaders(std::FILE* f, std::vector<Found>& out) {
    const uint64_t size = sizeOf(f);
    out.clear();
    uint64_t at = 0;
    uint8_t h[30];
    while (at + 30 <= size && readAt(f, at, h, sizeof h) && get32(h) == kLocalSig) {
        const uint32_t flags = get16(h + 6), method = get16(h + 8);
        const uint64_t compressed = get32(h + 18), plain = get32(h + 22);
        const size_t nameLen = get16(h + 26), extraLen = get16(h + 28);
        const uint64_t data = at + 30 + nameLen + extraLen;
        if ((flags & 8) != 0 || method != 0 || compressed != plain || data + compressed > size) break;
        Found e;
        e.name.resize(nameLen);
        if (nameLen && !readAt(f, at + 30, &e.name[0], nameLen)) break;
        e.header = at;
        e.compressed = e.size = compressed;
        e.crc = get32(h + 14);
        out.push_back(std::move(e));
        at = data + compressed;
    }
}

std::vector<uint8_t> localHeader(const std::string& name, uint64_t size, uint32_t crc) {
    std::vector<uint8_t> b;
    put32(b, kLocalSig);
    put16(b, 20);
    put16(b, 0);  // no flags
    put16(b, 0);  // stored
    put16(b, 0);
    put16(b, kDosDate);
    put32(b, crc);
    put32(b, uint32_t(size));
    put32(b, uint32_t(size));
    put16(b, uint32_t(name.size()));
    put16(b, 0);
    b.insert(b.end(), name.begin(), name.end());
    return b;
}

void centralEntry(std::vector<uint8_t>& b, const std::string& name, uint64_t header, uint64_t size, uint32_t crc) {
    const bool far = header >= 0xFFFFFFFF;
    put32(b, kCentralSig);
    put16(b, 45);
    put16(b, far ? 45 : 20);
    put16(b, 0);
    put16(b, 0);
    put16(b, 0);
    put16(b, kDosDate);
    put32(b, crc);
    put32(b, uint32_t(size));
    put32(b, uint32_t(size));
    put16(b, uint32_t(name.size()));
    put16(b, far ? 12 : 0);
    put16(b, 0);
    put16(b, 0);
    put16(b, 0);
    put32(b, 0);
    put32(b, far ? 0xFFFFFFFF : uint32_t(header));
    b.insert(b.end(), name.begin(), name.end());
    if (far) {
        put16(b, 1);
        put16(b, 8);
        put64(b, header);
    }
}

void endRecords(std::vector<uint8_t>& b, uint64_t count, uint64_t cdOffset, uint64_t cdSize) {
    const bool z64 = count >= 0xFFFF || cdOffset >= 0xFFFFFFFF || cdSize >= 0xFFFFFFFF;
    if (z64) {
        const uint64_t at = cdOffset + cdSize;  // the ZIP64 end record's own place
        put32(b, kEnd64Sig);
        put64(b, 44);
        put16(b, 45);
        put16(b, 45);
        put32(b, 0);
        put32(b, 0);
        put64(b, count);
        put64(b, count);
        put64(b, cdSize);
        put64(b, cdOffset);
        put32(b, kLocator64Sig);
        put32(b, 0);
        put64(b, at);
        put32(b, 1);
    }
    put32(b, kEndSig);
    put16(b, 0);
    put16(b, 0);
    put16(b, z64 ? 0xFFFF : uint32_t(count));
    put16(b, z64 ? 0xFFFF : uint32_t(count));
    put32(b, z64 ? 0xFFFFFFFF : uint32_t(cdSize));
    put32(b, z64 ? 0xFFFFFFFF : uint32_t(cdOffset));
    put16(b, 0);
}

}  // namespace

const char* groupName(Group g) { return g == Group::Sprites ? "sprites" : g == Group::Ui ? "ui" : "world"; }

std::string pictureName(Group g, const std::string& key) {
    std::string name = std::string(groupName(g)) + "/" + key + ".webp";
    std::replace(name.begin(), name.end(), '\\', '/');
    return name;
}

std::string indexText(const std::map<std::string, ai::Kind>& kinds) { return indexText(kinds, {}); }

std::string indexText(const std::map<std::string, ai::Kind>& kinds, const std::map<std::string, std::string>& aliases) {
    std::string t;
    for (const auto& e : kinds) t += e.first + "\t" + ai::kindName(e.second) + "\n";
    for (const auto& a : aliases) t += "=" + a.first + "\t" + a.second + "\n";
    return t;
}

std::map<std::string, std::string> parseAliases(const std::string& text) {
    std::map<std::string, std::string> out;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() < 2 || line[0] != '=') continue;
        const size_t tab = line.find('\t');
        if (tab == std::string::npos || tab < 2 || tab + 1 >= line.size()) continue;
        out[line.substr(1, tab - 1)] = line.substr(tab + 1);
    }
    return out;
}

std::map<std::string, ai::Kind> parseIndex(const std::string& text) {
    std::map<std::string, ai::Kind> out;
    for (const auto& e : parseIndexFile(text).pictures) out[e.first] = e.second.kind;
    return out;
}

std::string indexText(const Index& index) {
    std::string t;
    for (const auto& e : index.pictures) {
        t += e.first + "\t" + ai::kindName(e.second.kind);
        const char* sep = "\t";
        for (const std::string& map : e.second.maps) {
            t += sep + map;
            sep = ",";
        }
        t += "\n";
    }
    for (const auto& a : index.aliases) t += "=" + a.first + "\t" + a.second + "\n";
    for (const auto& n : index.alike) t += "~" + n.first + "\t" + n.second + "\n";
    for (const auto& e : index.pictures)  // 1.6
        if (!e.second.model.empty()) t += "^" + e.first + "\t" + e.second.model + "\t" + e.second.group + "\n";
    return t;
}

Index parseIndexFile(const std::string& text) {
    Index out;
    out.aliases = parseAliases(text);
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        std::string line = text.substr(at, end - at);
        at = end + 1;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const size_t tab = line.find('\t');
        if (tab == std::string::npos || tab == 0 || line[0] == '=') continue;  // 0.45: '=': a lower detail's key
        if (line[0] == '~') {  // 0.49: a picture found by its likeness
            if (tab > 1) out.alike[line.substr(1, tab - 1)] = line.substr(tab + 1);
            continue;
        }
        if (line[0] == '^') {  // 1.6: its model and group
            const std::string rest = line.substr(tab + 1);
            const size_t t2 = rest.find('\t');
            IndexEntry& e = out.pictures[line.substr(1, tab - 1)];
            e.model = rest.substr(0, t2);
            e.group = t2 == std::string::npos ? std::string() : rest.substr(t2 + 1);
            continue;
        }
        std::string kind = line.substr(tab + 1), maps;
        const size_t more = kind.find('\t');  // 0.48: the maps it was shown on
        if (more != std::string::npos) {
            maps = kind.substr(more + 1);
            kind.resize(more);
        }
        IndexEntry& e = out.pictures[line.substr(0, tab)];
        if (!ai::parseKind(kind, e.kind)) e.kind = ai::Kind::None;
        for (size_t from = 0; from <= maps.size();) {
            size_t comma = maps.find(',', from);
            if (comma == std::string::npos) comma = maps.size();
            if (comma > from) e.maps.insert(maps.substr(from, comma - from));
            from = comma + 1;
        }
    }
    return out;
}

bool keyOfPicture(const std::string& name, Group& group, std::string& key) {
    const size_t first = name.find('/');
    if (first == std::string::npos) return false;
    const std::string g = name.substr(0, first);
    Group found = Group::World;
    if (g == groupName(Group::World))
        found = Group::World;
    else if (g == groupName(Group::Sprites))
        found = Group::Sprites;
    else if (g == groupName(Group::Ui))
        found = Group::Ui;
    else
        return false;
    const size_t second = name.find('/', first + 1);
    const std::string ext = ".webp";
    if (second == std::string::npos || second == first + 1 || name.find('/', second + 1) != std::string::npos ||
        name.size() <= second + 1 + ext.size() || name.compare(name.size() - ext.size(), ext.size(), ext) != 0)
        return false;
    group = found;
    key = name.substr(first + 1, second - first - 1) + "\\" +
          name.substr(second + 1, name.size() - second - 1 - ext.size());
    return true;
}

bool Reader::open(const std::string& path, std::string& error) {
    close();
    file_ = std::fopen(path.c_str(), "rb");
    if (!file_) {
        error = "cannot open " + path;
        return false;
    }
    std::vector<Found> found;
    std::string why;
    if (!centralDirectory(file_, found, why)) {  // 0.48: cut short - its pictures by their local headers
        localHeaders(file_, found);
        if (found.empty()) {
            error = path + ": " + why;
            close();
            return false;
        }
        recovered_ = true;
    }
    entries_.reserve(found.size());
    for (Found& e : found) entries_[e.name] = Entry{e.header, e.compressed, e.size, uint16_t(e.method)};
    return true;
}

void Reader::close() {
    if (file_) std::fclose(file_);
    file_ = nullptr;
    recovered_ = false;
    entries_.clear();
}

uint64_t Reader::sizeOf(const std::string& name) const {
    const auto it = entries_.find(name);
    return it == entries_.end() ? 0 : it->second.compressed;
}

bool Reader::read(const std::string& name, std::vector<uint8_t>& out) const {
    const auto it = entries_.find(name);
    if (it == entries_.end() || !file_) return false;
    const Entry& e = it->second;
    if (e.size > (uint64_t(1) << 30) || e.compressed > (uint64_t(1) << 30) || (e.method != 0 && e.method != 8))
        return false;
    std::vector<uint8_t> raw(static_cast<size_t>(e.compressed));
    uint8_t h[30];
    if (!readAt(file_, e.header, h, sizeof h) || get32(h) != kLocalSig) return false;
    const uint64_t data = e.header + 30 + get16(h + 26) + get16(h + 28);
    if (!raw.empty() && !readAt(file_, data, raw.data(), raw.size())) return false;
    if (e.method == 0) {
        out = std::move(raw);
        return out.size() == e.size;
    }
    out.resize(static_cast<size_t>(e.size));
    const size_t n = tinfl_decompress_mem_to_mem(out.data(), out.size(), raw.data(), raw.size(), 0);
    return n == out.size();
}

std::vector<std::string> Reader::names() const {
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (const auto& e : entries_) out.push_back(e.first);
    std::sort(out.begin(), out.end());
    return out;
}

Writer::~Writer() {
    if (file_) std::fclose(file_);  // without a central directory: open(resume) finds the pictures again
}

bool Writer::open(const std::string& path, bool resume, std::string& error) {
    if (file_) std::fclose(file_);
    file_ = nullptr;
    path_ = path;
    entries_.clear();
    names_.clear();
    end_ = 0;
    if (resume) file_ = std::fopen(path.c_str(), "r+b");
    if (file_) {
        std::vector<Found> found;
        std::string why;
        if (!centralDirectory(file_, found, why)) localHeaders(file_, found);
        size_t last = 0;  // the picture furthest in the file
        for (Found& e : found) {
            if (e.name == kIndexName || e.method != 0 || names_.count(e.name)) continue;  // the index is made anew
            if (!entries_.empty() && e.header > entries_[last].header) last = entries_.size();
            names_[e.name] = entries_.size();
            entries_.push_back(Central{e.name, e.header, e.size, e.crc});
        }
        if (!entries_.empty()) {  // the end of the last picture, by its local header
            uint8_t h[30];
            const Central& c = entries_[last];
            if (!readAt(file_, c.header, h, sizeof h) || get32(h) != kLocalSig) {
                error = path + ": its last picture cannot be read";
                std::fclose(file_);
                file_ = nullptr;
                return false;
            }
            end_ = c.header + 30 + get16(h + 26) + get16(h + 28) + c.size;
        }
        if (!truncateTo(file_, end_)) {
            error = "cannot shorten " + path;
            std::fclose(file_);
            file_ = nullptr;
            return false;
        }
        return true;
    }
    file_ = std::fopen(path.c_str(), "w+b");
    if (!file_) {
        error = "cannot create " + path;
        return false;
    }
    return true;
}

std::vector<std::string> Writer::names() const {
    std::vector<std::string> out;
    out.reserve(entries_.size());
    for (const Central& c : entries_) out.push_back(c.name);
    return out;
}

bool Writer::writeLocal(const std::string& name, const uint8_t* data, size_t size, uint32_t crc, std::string& error) {
    const std::vector<uint8_t> h = localHeader(name, size, crc);
    if (!seekTo(file_, end_) || std::fwrite(h.data(), 1, h.size(), file_) != h.size() ||
        (size && std::fwrite(data, 1, size, file_) != size)) {
        error = "cannot write " + path_ + " (the disk is full?)";
        return false;
    }
    return true;
}

bool Writer::add(const std::string& name, const uint8_t* data, size_t size, std::string& error, bool replace) {
    if (!file_) {
        error = "the pack is not open";
        return false;
    }
    if (has(name) && !replace) return true;
    if (name.empty() || name.size() > 0xFFFF || uint64_t(size) >= 0xFFFFFFFF || name == kIndexName) {
        error = "not a picture of the pack: " + name;
        return false;
    }
    const uint32_t crc = uint32_t(mz_crc32(MZ_CRC32_INIT, data, size));
    if (!writeLocal(name, data, size, crc, error)) return false;
    const auto there = names_.find(name);
    if (there != names_.end()) {
        entries_[there->second] = Central{name, end_, uint64_t(size), crc};
    } else {
        names_[name] = entries_.size();
        entries_.push_back(Central{name, end_, uint64_t(size), crc});
    }
    end_ += 30 + name.size() + size;
    return true;
}

bool Writer::remove(const std::string& name) {
    const auto it = names_.find(name);
    if (it == names_.end()) return false;
    const size_t at = it->second;
    names_.erase(it);
    if (at + 1 != entries_.size()) {  // the last one into its place: the directory's order means nothing
        entries_[at] = std::move(entries_.back());
        names_[entries_[at].name] = at;
    }
    entries_.pop_back();
    return true;
}

bool Writer::checkpoint(const std::string& index, std::string& error) {
    if (!file_) {
        error = "the pack is not open";
        return false;
    }
    uint64_t at = end_;
    std::vector<uint8_t> cd;
    for (const Central& c : entries_) centralEntry(cd, c.name, c.header, c.size, c.crc);
    uint64_t count = entries_.size();
    if (!index.empty()) {  // after the pictures, so that the next add() goes over it
        const auto* data = reinterpret_cast<const uint8_t*>(index.data());
        const uint32_t crc = uint32_t(mz_crc32(MZ_CRC32_INIT, data, index.size()));
        if (!writeLocal(kIndexName, data, index.size(), crc, error)) return false;
        centralEntry(cd, kIndexName, at, index.size(), crc);
        at += 30 + std::strlen(kIndexName) + index.size();
        ++count;
    }
    endRecords(cd, count, at, cd.size());
    if (!seekTo(file_, at) || std::fwrite(cd.data(), 1, cd.size(), file_) != cd.size() ||
        !truncateTo(file_, at + cd.size())) {
        error = "cannot write " + path_ + " (the disk is full?)";
        return false;
    }
    return std::fflush(file_) == 0;
}

bool Writer::close(const std::string& index, std::string& error) {
    if (!file_) return true;
    const bool ok = checkpoint(index, error);
    std::fclose(file_);
    file_ = nullptr;
    return ok;
}

bool rewrite(const std::string& from, const std::string& to, const std::function<bool(const std::string&)>& keep,
             size_t& kept, std::string& error) {
    kept = 0;
    Reader r;
    if (!r.open(from, error)) return false;
    std::vector<uint8_t> data;
    const Index old = r.read(kIndexName, data) ? parseIndexFile(std::string(data.begin(), data.end())) : Index();
    Writer w;
    if (!w.open(to, false, error)) return false;
    Index now;
    for (const std::string& name : r.names()) {
        if (name == kIndexName || !keep(name)) continue;
        if (!r.read(name, data)) {
            error = from + ": " + name + " cannot be read";
            return false;
        }
        if (!w.add(name, data.data(), data.size(), error)) return false;
        const auto e = old.pictures.find(name);
        if (e != old.pictures.end()) now.pictures[name] = e->second;
        ++kept;
    }
    for (const auto& a : old.aliases)
        if (w.has(a.second)) now.aliases.insert(a);
    return w.close(indexText(now), error);
}

}  // namespace uagfx::pack
