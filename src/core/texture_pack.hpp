#pragma once
#include <cstdint>
#include <cstdio>
#include <map>
#include <memory>
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
std::string indexText(const std::map<std::string, ai::Kind>& kinds);  // 0.43: the names and kinds
std::map<std::string, ai::Kind> parseIndex(const std::string& text);  // a kind it does not know: ai::Kind::None

class Reader {
public:
    Reader() = default;
    ~Reader() { close(); }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    bool open(const std::string& path, std::string& error);  // the file stays open until close()
    void close();
    bool isOpen() const { return file_ != nullptr; }
    size_t count() const { return entries_.size(); }  // the index entry too
    bool has(const std::string& name) const { return entries_.count(name) != 0; }
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

}  // namespace uagfx::pack
