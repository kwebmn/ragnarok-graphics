#pragma once
#include <cstdint>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace uagfx::grf {

// Ragnarok's GRF archive, version 0x200: a 46-byte header, the files' zlib streams, and a zlib-compressed table of
// names, sizes and offsets that the header points to. upscaled.grf (the neural texture cache) is one: GRF tools open
// it, and copying it to another player's client hands them the finished textures.
struct Entry {
    uint32_t compressed = 0;  // bytes of the zlib stream
    uint32_t aligned = 0;     // as stored (equal for unencrypted files)
    uint32_t size = 0;        // bytes once inflated
    uint8_t flags = 1;        // 1: a file; 2/4: DES-encrypted (refused)
    uint32_t offset = 0;      // from the end of the header
};

class Archive {
public:
    Archive() = default;
    ~Archive();
    Archive(const Archive&) = delete;
    Archive& operator=(const Archive&) = delete;

    // An existing archive, or a new empty one when create and the file is not there. Names are compared as stored.
    bool open(const std::string& path, bool create, std::string& error);
    void close();
    bool isOpen() const { return file_ != nullptr; }
    const Entry* find(const std::string& name) const;
    size_t count() const { return entries_.size(); }
    bool read(const std::string& name, std::vector<uint8_t>& out, std::string& error);
    // Compresses and adds the files (a name already there gets the new data), then a new table and header. New data
    // goes after the old table, which stays in place until the header points elsewhere: an archive cut short by a
    // crash still opens as it was. Fails rather than pass the format's 4 GB.
    bool append(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files, std::string& error);
    uint64_t bytes() const { return end_; }  // where the next data would go: the archive's length

private:
    bool writeTable(uint64_t at, std::string& error);

    FILE* file_ = nullptr;
    bool writable_ = false;
    std::map<std::string, Entry> entries_;
    uint64_t end_ = 46;  // end of the last table written or read
};

}  // namespace uagfx::grf
