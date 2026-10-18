#include "core/grf/grf.hpp"

#include <algorithm>
#include <cstring>

#include "miniz.h"

namespace uagfx::grf {
namespace {

constexpr char kMagic[16] = "Master of Magic";  // and its NUL
constexpr uint32_t kVersion = 0x200;
constexpr uint64_t kHeader = 46;
constexpr uint64_t kLimit = 0xFFFFFFFFull;  // offsets and sizes are 32-bit

void put32(uint8_t* p, uint32_t v) {
    p[0] = uint8_t(v);
    p[1] = uint8_t(v >> 8);
    p[2] = uint8_t(v >> 16);
    p[3] = uint8_t(v >> 24);
}
uint32_t get32(const uint8_t* p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 | uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}

bool seek(FILE* f, uint64_t at) {
#ifdef _WIN32
    return _fseeki64(f, int64_t(at), SEEK_SET) == 0;
#else
    return fseeko(f, off_t(at), SEEK_SET) == 0;
#endif
}

bool readAt(FILE* f, uint64_t at, void* data, size_t n) { return seek(f, at) && std::fread(data, 1, n, f) == n; }
bool writeAt(FILE* f, uint64_t at, const void* data, size_t n) {
    return seek(f, at) && std::fwrite(data, 1, n, f) == n;
}

}  // namespace

Archive::~Archive() { close(); }

void Archive::close() {
    if (file_) std::fclose(file_);
    file_ = nullptr;
    entries_ = std::make_shared<Table>();  // another reader of the old table keeps it
    path_.clear();
    end_ = kHeader;
}

Archive::Table& Archive::own() {
    if (entries_.use_count() > 1) entries_ = std::make_shared<Table>(*entries_);
    return *entries_;
}

bool Archive::openLike(const Archive& other, std::string& error) {
    close();
    if (!other.file_ || other.path_.empty()) {
        error = "no archive open by its path to read again";
        return false;
    }
    file_ = std::fopen(other.path_.c_str(), "rb");
    if (!file_) {
        error = "cannot open " + other.path_;
        return false;
    }
    writable_ = false;
    path_ = other.path_;
    entries_ = other.entries_;
    end_ = other.end_;
    return true;
}

bool Archive::open(const std::string& path, bool create, std::string& error) {
    close();
    file_ = std::fopen(path.c_str(), "r+b");
    writable_ = file_ != nullptr;
    if (!file_) file_ = std::fopen(path.c_str(), "rb");
    if (!file_) {
        if (!create) {
            error = "cannot open " + path;
            return false;
        }
        file_ = std::fopen(path.c_str(), "w+b");
        if (!file_) {
            error = "cannot create " + path;
            return false;
        }
        writable_ = true;
        if (!writeTable(kHeader, error)) {  // an empty table: the file is a valid archive from the start
            close();
            return false;
        }
        path_ = path;
        return true;
    }
    if (!readTable(path, error)) return false;
    path_ = path;
    return true;
}

bool Archive::openToRead(const std::string& path, std::string& error) {
    close();
    file_ = std::fopen(path.c_str(), "rb");
    if (!file_) {
        error = "cannot open " + path;
        return false;
    }
    writable_ = false;
    if (!readTable(path, error)) return false;
    path_ = path;
    return true;
}

bool Archive::open(std::FILE* file, bool writable, std::string& error) {
    close();
    if (!file) {
        error = "no file";
        return false;
    }
    file_ = file;
    writable_ = writable;
    return readTable("the archive", error);
}

bool Archive::readTable(const std::string& path, std::string& error) {
    uint8_t h[kHeader];
    if (!readAt(file_, 0, h, sizeof h) || std::memcmp(h, kMagic, 15) != 0 || get32(h + 42) != kVersion) {
        error = path + " is not a GRF archive of version 0x200";
        close();
        return false;
    }
    const uint64_t at = kHeader + get32(h + 30);
    uint8_t sizes[8];
    if (!readAt(file_, at, sizes, sizeof sizes)) {
        error = path + ": no file table";
        close();
        return false;
    }
    const uint32_t packed = get32(sizes), unpacked = get32(sizes + 4);
    std::vector<uint8_t> z(packed), table(unpacked);
    mz_ulong n = unpacked;
    if (!readAt(file_, at + 8, z.data(), z.size()) ||
        (unpacked && (mz_uncompress(table.data(), &n, z.data(), mz_ulong(z.size())) != MZ_OK || n != unpacked))) {
        error = path + ": damaged file table";
        close();
        return false;
    }
    Table entries;
    for (size_t p = 0; p < table.size();) {
        const void* nul = std::memchr(table.data() + p, 0, table.size() - p);
        if (!nul) break;
        const size_t len = size_t(static_cast<const uint8_t*>(nul) - (table.data() + p));
        if (p + len + 1 + 17 > table.size()) break;
        std::string name(reinterpret_cast<const char*>(table.data() + p), len);
        const uint8_t* e = table.data() + p + len + 1;
        Entry x;
        x.compressed = get32(e);
        x.aligned = get32(e + 4);
        x.size = get32(e + 8);
        x.flags = e[12];
        x.offset = get32(e + 13);
        entries[name] = x;
        p += len + 1 + 17;
    }
    entries_ = std::make_shared<Table>(std::move(entries));
    end_ = at + 8 + packed;
    return true;
}

std::vector<std::string> Archive::names() const {
    std::vector<std::string> out;
    out.reserve(entries_->size());
    for (const auto& e : *entries_) out.push_back(e.first);
    return out;
}

const Entry* Archive::find(const std::string& name) const {
    const auto it = entries_->find(name);
    return it == entries_->end() ? nullptr : &it->second;
}

bool Archive::read(const std::string& name, std::vector<uint8_t>& out, std::string& error) {
    const Entry* e = find(name);
    if (!file_ || !e) {
        error = "no " + name + " in the archive";
        return false;
    }
    if (!(e->flags & 1) || (e->flags & 6)) {
        error = name + " is a folder or encrypted";
        return false;
    }
    std::vector<uint8_t> z(e->aligned);
    out.resize(e->size);
    mz_ulong n = e->size;
    if (!readAt(file_, kHeader + e->offset, z.data(), z.size()) ||
        mz_uncompress(out.data(), &n, z.data(), mz_ulong(e->compressed)) != MZ_OK || n != e->size) {
        error = name + ": damaged data";
        return false;
    }
    return true;
}

bool Archive::append(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files, std::string& error) {
    if (!file_ || !writable_) {
        error = "the archive is not open for writing";
        return false;
    }
    uint64_t at = end_;
    std::map<std::string, Entry> added;
    for (const auto& f : files) {
        mz_ulong n = mz_compressBound(mz_ulong(f.second.size()));
        std::vector<uint8_t> z(n);
        if (mz_compress2(z.data(), &n, f.second.data(), mz_ulong(f.second.size()), MZ_BEST_SPEED) != MZ_OK) {
            error = "cannot compress " + f.first;
            return false;
        }
        if (at - kHeader + n > kLimit) {
            error = "the archive would pass 4 GB";
            return false;
        }
        if (!writeAt(file_, at, z.data(), n)) {
            error = "cannot write " + f.first;
            return false;
        }
        Entry e;
        e.compressed = e.aligned = uint32_t(n);
        e.size = uint32_t(f.second.size());
        e.offset = uint32_t(at - kHeader);
        added[f.first] = e;
        at += n;
    }
    const Table was = *entries_;
    Table& table = own();
    for (const auto& a : added) table[a.first] = a.second;
    if (!writeTable(at, error)) {
        own() = was;  // the header still points to the old table
        return false;
    }
    return true;
}

bool Archive::copyTo(std::FILE* out, const std::function<bool(const std::string&)>& keep, std::string& error) {
    if (!file_ || !out) {
        error = "no archive to copy";
        return false;
    }
    // In the order of the data: the source is read front to back.
    std::vector<std::pair<uint32_t, std::string>> order;
    for (const auto& e : *entries_)
        if (keep(e.first)) order.emplace_back(e.second.offset, e.first);
    std::sort(order.begin(), order.end());
    Archive copy;
    copy.file_ = out;
    copy.writable_ = true;
    uint64_t at = kHeader;
    std::vector<uint8_t> data;
    for (const auto& o : order) {
        Entry e = entries_->at(o.second);
        data.resize(e.aligned);
        if (!readAt(file_, kHeader + e.offset, data.data(), data.size())) {
            error = "cannot read " + o.second;
            copy.file_ = nullptr;
            return false;
        }
        if (!writeAt(out, at, data.data(), data.size())) {
            error = "cannot write " + o.second;
            copy.file_ = nullptr;
            return false;
        }
        e.offset = uint32_t(at - kHeader);
        copy.own()[o.second] = e;
        at += data.size();
    }
    const bool ok = copy.writeTable(at, error);
    copy.file_ = nullptr;  // the caller closes it
    return ok;
}

bool Archive::writeTable(uint64_t at, std::string& error) {
    std::vector<uint8_t> table;
    for (const auto& e : *entries_) {
        table.insert(table.end(), e.first.begin(), e.first.end());
        table.push_back(0);
        uint8_t b[17];
        put32(b, e.second.compressed);
        put32(b + 4, e.second.aligned);
        put32(b + 8, e.second.size);
        b[12] = e.second.flags;
        put32(b + 13, e.second.offset);
        table.insert(table.end(), b, b + 17);
    }
    mz_ulong n = mz_compressBound(mz_ulong(table.size()));
    std::vector<uint8_t> z(8 + n);
    if (mz_compress2(z.data() + 8, &n, table.data(), mz_ulong(table.size()), MZ_DEFAULT_COMPRESSION) != MZ_OK ||
        at - kHeader + 8 + n > kLimit) {
        error = "cannot write the file table";
        return false;
    }
    put32(z.data(), uint32_t(n));
    put32(z.data() + 4, uint32_t(table.size()));
    uint8_t h[kHeader] = {};
    std::memcpy(h, kMagic, 16);
    put32(h + 30, uint32_t(at - kHeader));
    put32(h + 34, 0);                                   // seed
    put32(h + 38, uint32_t(entries_->size()) + 7);      // count + seed + 7
    put32(h + 42, kVersion);
    // The table first, the header that points to it last: until then the old header and table stay valid.
    if (!writeAt(file_, at, z.data(), 8 + n) || std::fflush(file_) != 0 || !writeAt(file_, 0, h, sizeof h) ||
        std::fflush(file_) != 0) {
        error = "cannot write the file table";
        return false;
    }
    end_ = at + 8 + n;
    return true;
}

}  // namespace uagfx::grf
