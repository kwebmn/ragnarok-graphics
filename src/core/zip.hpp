#pragma once
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace uagfx::zip {

// 0.25: reading the client's zip archives (DATA.INI lists them with its GRF; data.zip holds the maps): the central
// directory's entries a filter keeps, read by name - stored or deflated, zip64 too. Read-only.
std::string normalName(const std::string& name);  // lower case, backslashes: as the game names its files

class Index {
public:
    bool open(const std::string& path, const std::function<bool(const std::string&)>& keep, std::string& error);
    size_t count() const { return entries_.size(); }
    bool has(const std::string& name) const { return entries_.count(name) != 0; }  // a normal name
    // Opens the archive again: an Index holds no file.
    bool read(const std::string& name, std::vector<uint8_t>& out, std::string& error) const;

private:
    struct Entry {
        uint64_t local = 0;  // its local header
        uint64_t compressed = 0;
        uint64_t size = 0;
        uint16_t method = 0;  // 0 stored, 8 deflated
    };
    std::string path_;
    std::unordered_map<std::string, Entry> entries_;
};

}  // namespace uagfx::zip
