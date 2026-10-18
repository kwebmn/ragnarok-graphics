#pragma once
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace uagfx::grf {

// Ragnarok's GRF archive, version 0x200: a 46-byte header, the files' zlib streams, and a zlib-compressed table of
// names, sizes and offsets that the header points to: the game's data.grf; upscaled.grf (the neural texture cache of
// 0.11-0.47, the texture pack since 0.48) was one too.
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
    // An archive in a file the caller opened ("rb", or "r+b" when writable; the settings app opens UTF-16 paths):
    // the archive closes it.
    bool open(std::FILE* file, bool writable, std::string& error);
    // 2.3.1: a game's archive, for reading: no write access asked for. The game opens its archives sharing them for
    // reading alone (uaRO.exe 0x510b50: FILE_SHARE_READ): a handle of ours that could write, held while uaRO Studio
    // read the archives, kept it from opening them ("Unable to load table class_tab.txt").
    bool openToRead(const std::string& path, std::string& error);
    bool writable() const { return writable_; }
    // 2.3: the archive `other` has open (by its path), read-only, with a file handle of its own and other's table
    // shared - a reading thread each (the texture pack's builder, the studio's catalog), the table read once.
    bool openLike(const Archive& other, std::string& error);
    void close();
    bool isOpen() const { return file_ != nullptr; }
    const Entry* find(const std::string& name) const;
    size_t count() const { return entries_->size(); }
    std::vector<std::string> names() const;
    bool read(const std::string& name, std::vector<uint8_t>& out, std::string& error);
    // Compresses and adds the files (a name already there gets the new data), then a new table and header. New data
    // goes after the old table, which stays in place until the header points elsewhere: an archive cut short by a
    // crash still opens as it was. Fails rather than pass the format's 4 GB.
    bool append(const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files, std::string& error);
    uint64_t bytes() const { return end_; }  // where the next data would go: the archive's length
    // A new archive in `out` (opened "wb") with the entries `keep` takes, their stored data copied as it is, and the
    // space of old file tables gone (the settings app deleted pictures of upscaled.grf so until 0.48).
    bool copyTo(std::FILE* out, const std::function<bool(const std::string&)>& keep, std::string& error);

private:
    bool readTable(const std::string& what, std::string& error);
    bool writeTable(uint64_t at, std::string& error);

    using Table = std::map<std::string, Entry>;
    Table& own();  // the table to change: a copy of its own first when it is shared (openLike)

    FILE* file_ = nullptr;
    bool writable_ = false;
    std::string path_;  // 2.3: "" when opened by a FILE*
    std::shared_ptr<Table> entries_ = std::make_shared<Table>();
    uint64_t end_ = 46;  // end of the last table written or read
};

}  // namespace uagfx::grf
