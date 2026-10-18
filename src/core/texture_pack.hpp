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
// 1.6: and the model that made it and its group (ai::groupName) - "^name\tmodel\tgroup" lines, which an older
// index reader takes for a picture of no kind that the pack does not have.
struct IndexEntry {
    ai::Kind kind = ai::Kind::None;
    std::set<std::string> maps;
    std::string model;  // "" not known: made before 1.6
    std::string group;
    bool operator==(const IndexEntry& o) const {
        return kind == o.kind && maps == o.maps && model == o.model && group == o.group;
    }
};
// 2.3, uaRO Studio's lines (an older reader takes each for a picture of no kind the pack has not):
// "+<picture>\t<file>\t<archive>\t<made>\t<model>\t<chain>" - what a picture was made from, when, by what;
// "!<key>" - the game's own texture always (no picture made or shown for it);
// "#<key>\tgloss=<x>\trelief=<x>\theight=<x>" - the driver's material of that texture scaled (0 turns a part off).
struct Provenance {
    std::string file, archive;  // the game's file (its CP949 name) and the archive that gave it
    int64_t made = 0;           // Unix seconds
    std::string model, chain;   // the network's model; the processing chain (ai::chainText)
    bool operator==(const Provenance& o) const {
        return file == o.file && archive == o.archive && made == o.made && model == o.model && chain == o.chain;
    }
};
struct MaterialScales {
    float gloss = 1, relief = 1, height = 1;
    bool operator==(const MaterialScales& o) const {
        return gloss == o.gloss && relief == o.relief && height == o.height;
    }
};
struct Index {
    std::map<std::string, IndexEntry> pictures;
    std::map<std::string, std::string> aliases;  // a key of a lower texture detail -> its whole texture's picture
    // 0.49: the pictures the game finds by their likeness ("~name\t<likenessText>" lines, near_picture.hpp).
    std::map<std::string, std::string> alike;
    std::map<std::string, Provenance> made;           // 2.3: a picture's name ->
    std::set<std::string> ownAlways;                  // 2.3: keys
    std::map<std::string, MaterialScales> materials;  // 2.3: keys ->
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
    uint16_t dosDate = 0, dosTime = 0;  // 2.3: when it was written (MS-DOS fields, UTC)
};
// 2.3: the pack's entries carry the time they were written (they had 1980-01-01, which stays "no time": 0).
int64_t unixOfDos(uint16_t date, uint16_t time);
void dosOfUnix(int64_t t, uint16_t& date, uint16_t& time);
using ReadAt = std::function<bool(uint64_t at, uint8_t* out, size_t n)>;
bool centralDirectoryOf(uint64_t size, const ReadAt& read, std::vector<ZipEntry>& out, std::string& error);
// 2.2.5: the pictures a pack's central directory names (its index not counted), by its end records and its directory
// alone - no name kept, never every picture's header: false (and why) for a pack being written, which has no directory
// until its builder's checkpoint. The settings app's line about the pack froze its window reading those.
bool countOf(const std::string& path, uint64_t& pictures, std::string& error);

class Reader {
public:
    Reader() = default;
    ~Reader() { close(); }
    Reader(const Reader&) = delete;
    Reader& operator=(const Reader&) = delete;

    // The file stays open until close(). 0.48: a pack cut short before its central directory (its builder or the
    // game's own making stopped) is read by its pictures' local headers, without its index: recovered(). 2.2.5:
    // recover false - not that (the settings app: every header of a pack being written, read on its window's thread,
    // froze it); false then.
    bool open(const std::string& path, std::string& error, bool recover = true);
    void close();
    bool isOpen() const { return file_ != nullptr; }
    bool recovered() const { return recovered_; }
    size_t count() const { return entries_.size(); }  // the index entry too
    bool has(const std::string& name) const { return entries_.count(name) != 0; }
    uint64_t sizeOf(const std::string& name) const;  // its bytes in the pack; 0 when it has none
    int64_t timeOf(const std::string& name) const;   // 2.3: when it was written (Unix seconds); 0 none, or no time
    // A file of the pack, stored or deflated. One thread at a time (the renderer's I/O thread, the builder's).
    bool read(const std::string& name, std::vector<uint8_t>& out) const;
    std::vector<std::string> names() const;

private:
    struct Entry {
        uint64_t header = 0;  // its local header
        uint64_t compressed = 0;
        uint64_t size = 0;
        uint16_t method = 0;  // 0 stored, 8 deflated
        uint16_t date = 0, time = 0;
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
    // replace (1.6): a picture already there gets the new one - its directory entry points at the new bytes, the old
    // stay unread until the Ready textures page rewrites the pack; else a name there is left as it is.
    // when (2.3): the entry's time - Unix seconds, -1 now, 0 none (1980-01-01).
    bool add(const std::string& name, const uint8_t* data, size_t size, std::string& error, bool replace = false,
             int64_t when = -1);
    // 2.2.5: a picture out of the directory (one map's, to test a model there) - its bytes stay unread until the Ready
    // textures page rewrites the pack; false when it is not there.
    bool remove(const std::string& name);
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
        uint16_t date = 0x21, time = 0;
    };
    bool writeLocal(const Central& c, const uint8_t* data, std::string& error);
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
