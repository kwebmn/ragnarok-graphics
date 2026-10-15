#pragma once
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/ai/kinds.hpp"

namespace uagfx::pack {

// 0.42: the texture pack - uaro-textures.zip next to uaRO.exe: the AI's pictures (4 times the game's textures, WebP)
// that a player's game shows without making any. One ZIP64 file whose pictures are stored as they are (WebP is
// compressed already): a picture is one read at its place, the central directory read once at the start its index.
// The pack builder (the settings app) makes it; 7-Zip and Explorer open it too.

// What a picture is made for: the world's textures (their edges wrap), the sprite caches' frames (nothing past their
// edges) and the interface's pictures (their edges go on) - the network makes each in its own way.
enum class Group : uint8_t { World, Sprites, Ui };
const char* groupName(Group g);  // "world", "sprites", "ui"
// The picture of a game texture's key (ai::keyName, "256x256\0123456789abcdef"):
// "world/256x256/0123456789abcdef.webp".
std::string pictureName(Group g, const std::string& key);
// 0.44: back from a picture's name to its group and key; false for any other name.
bool keyOfPicture(const std::string& name, Group& group, std::string& key);
// A line a picture - its name, its kind (ai::kindName), the maps it is on, by tabs and commas; written last.
constexpr const char* kIndexName = "index.txt";
// 0.48: held by whoever writes the pack - its builder, or a game making pictures ([ai] generate) - one at a time; the
// settings app changes no pack while it is held.
constexpr const char* kWriterMutex = "Local\\uaro-textures-zip";
std::string indexText(const std::map<std::string, ai::Kind>& kinds);  // 0.43: the names and kinds
// 0.45: and the keys of the textures at a lower detail, each to its whole texture's picture ("=<key>\t<picture>").
std::string indexText(const std::map<std::string, ai::Kind>& kinds, const std::map<std::string, std::string>& aliases);
std::map<std::string, ai::Kind> parseIndex(const std::string& text);  // a kind it does not know: ai::Kind::None
std::map<std::string, std::string> parseAliases(const std::string& text);
// 0.48: a picture's line whole - its kind and the maps the game showed it on (the game's own making notes them, the
// builder's pictures have none) - and the index whole: "name\tkind\tmap,map" lines, then the aliases' lines.
struct IndexEntry {
    ai::Kind kind = ai::Kind::None;
    std::set<std::string> maps;
    bool operator==(const IndexEntry& o) const { return kind == o.kind && maps == o.maps; }
};
struct Index {
    std::map<std::string, IndexEntry> pictures;
    std::map<std::string, std::string> aliases;  // a key of a lower texture detail -> its whole texture's picture
    // 0.49: the pictures the game finds by their likeness ("~name\t<likenessText>" lines, near_picture.hpp).
    std::map<std::string, std::string> alike;
};
std::string indexText(const Index& index);
Index parseIndexFile(const std::string& text);

// 0.53: a ZIP's entry as its central directory has it; the directory read through a function of the bytes at an
// offset (a file, or a pack on the site by HTTP ranges).
struct ZipEntry {
    std::string name;
    uint64_t header = 0, compressed = 0, size = 0;  // its local header's offset, its sizes
    uint32_t crc = 0;
    uint32_t method = 0;  // 0 stored
};
using ReadAt = std::function<bool(uint64_t at, uint8_t* out, size_t n)>;
bool centralDirectoryOf(uint64_t size, const ReadAt& read, std::vector<ZipEntry>& out, std::string& error);

class Reader {
public:
    Reader() = default;
    ~Reader() { close(); }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    // The file stays open until close(). 0.48: a pack cut short before its central directory (its builder or the
    // game's own making stopped) is read by its pictures' local headers, without its index: recovered().
    bool open(const std::string& path, std::string& error);
    void close();
    bool isOpen() const { return file_ != nullptr; }
    bool recovered() const { return recovered_; }
    size_t count() const { return entries_.size(); }  // the index entry too
    bool has(const std::string& name) const { return entries_.count(name) != 0; }
    uint64_t sizeOf(const std::string& name) const;  // its bytes in the pack; 0 when it has none
    // A file of the pack, stored or deflated. One thread at a time (the renderer's I/O thread, the builder's).
    bool read(const std::string& name, std::vector<uint8_t>& out) const;
    std::vector<std::string> names() const;

private:
    struct Entry {
        uint64_t header = 0;  // its local header
        uint64_t compressed = 0;
        uint64_t size = 0;
        uint16_t method = 0;  // 0 stored, 8 deflated
    };
    std::FILE* file_ = nullptr;
    bool recovered_ = false;
    std::unordered_map<std::string, Entry> entries_;
};

class Writer {
public:
    Writer() = default;
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    // A new pack in place of what is there; or (resume) the one there goes on: its pictures stay - also when its last
    // run stopped before writing its central directory (they are found by their local headers then), its index goes.
    bool open(const std::string& path, bool resume, std::string& error);
    bool isOpen() const { return file_ != nullptr; }
    bool has(const std::string& name) const { return names_.count(name) != 0; }
    size_t count() const { return entries_.size(); }
    uint64_t bytes() const { return end_; }  // the pictures so far
    std::vector<std::string> names() const;
    bool add(const std::string& name, const uint8_t* data, size_t size, std::string& error);
    // The index (when not empty) and the central directory after the last picture: a whole ZIP from here on - the next
    // add() goes over them.
    bool checkpoint(const std::string& index, std::string& error);
    bool close(const std::string& index, std::string& error);  // checkpoint, then the file closes

private:
    struct Central {
        std::string name;
        uint64_t header = 0;
        uint64_t size = 0;
        uint32_t crc = 0;
    };
    bool writeLocal(const std::string& name, const uint8_t* data, size_t size, uint32_t crc, std::string& error);
    std::FILE* file_ = nullptr;
    std::string path_;
    uint64_t end_ = 0;  // after the last picture's data
    std::vector<Central> entries_;
    std::unordered_map<std::string, size_t> names_;
};

// 0.48: the pack at `from` again at `to` with the pictures `keep` takes (copied as they are) and the index of what
// stays (a lower detail's key to a picture that goes, goes too) - the Ready textures page deletes pictures so.
// kept: the pictures copied.
bool rewrite(const std::string& from, const std::string& to, const std::function<bool(const std::string&)>& keep,
             size_t& kept, std::string& error);

}  // namespace uagfx::pack
