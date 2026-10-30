#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace uagfx::zip {

// 0.25: reading the client's zip archives (DATA.INI lists them with its GRF; data.zip holds the maps): the central
// directory's entries a filter keeps, read by name - stored or deflated, zip64 too. Read-only.
std::string normalName(const std::string& name);  // lower case, backslashes: as the game names its files

// 3.0.11-beta.76: where an entry is in its archive (what read() needs of it)
struct Place {
    uint64_t local = 0;  // its local header
    uint64_t compressed = 0;
    uint64_t size = 0;
    uint16_t method = 0;  // 0 stored, 8 deflated
};

class Index {
public:
    bool open(const std::string& path, const std::function<bool(const std::string&)>& keep, std::string& error);
    // 3.0.11-beta.76: and each entry `keep` does not keep told to `rest` with its place (an index of one's own)
    bool open(const std::string& path, const std::function<bool(const std::string&)>& keep, std::string& error,
              const std::function<void(const std::string&, const Place&)>& rest);
    // 2.10: an archive in memory (the setup's appended package, a download read whole); read() reads from it.
    bool openBytes(std::string bytes, const std::function<bool(const std::string&)>& keep, std::string& error);
    size_t count() const { return entries_.size(); }
    bool has(const std::string& name) const { return entries_.count(name) != 0; }  // a normal name
    std::vector<std::string> names() const;  // 0.43: the normal names kept
    // Opens the archive again: an Index holds no file.
    bool read(const std::string& name, std::vector<uint8_t>& out, std::string& error) const;
    bool read(const Place& place, std::vector<uint8_t>& out, std::string& error) const;  // 3.0.11-beta.76

private:
    using ReadAt = std::function<bool(uint64_t at, void* data, size_t n)>;
    using Rest = std::function<void(const std::string&, const Place&)>;
    bool scan(uint64_t size, const ReadAt& readAt, const std::function<bool(const std::string&)>& keep,
              std::string& error, const Rest& rest = nullptr);
    using Entry = Place;
    std::string path_;
    std::shared_ptr<const std::string> bytes_;  // openBytes'; else the file at path_
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace uagfx::zip
