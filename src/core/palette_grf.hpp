#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace uagfx::palettes {

// 2.5.19: the client's Palettes.grf with the palettes the server lets be worn, made on the player's computer (the
// Service page) from the whole set in seconds - Kamishi's pack: 219,293 palettes, 153 MB; about 18,500 of them stay,
// in 4-5 MB. uaRO.exe of 2007 asks for data\palette\머리\머리<style>_<sex>_<colour>.pal and
// data\palette\몸\<job>_<sex>_<colour>.pal, and draws the sprite's own colours when it finds none - no error.
// The server's limits (its stylist npc/custom/dye.txt; uAthena's pc_changelook clamps every look change to them):
constexpr int kMaxHairStyle = 23;
constexpr int kMaxHairColour = 20;
constexpr int kMaxClothColour = 77;

// The name the client asks for of a file in a source - its path in a GRF, a zip or a folder, cp949 bytes, '/' or
// '\': data\palette\<the rest> after its palette folder; "" when it is in none.
std::string clientName(const std::string& path);

// Why a palette stays out: hair past the styles or colours, clothes past the colours, a folder the 2007 client knows
// nothing of (costumes, the Doram race), a name it never asks for, an empty file, a file that is no palette.
enum class Left : uint8_t { HairPast, ClothesPast, Unknown, OddName, Empty, NotPalette };
constexpr int kLeftKinds = 6;
std::optional<Left> verdict(const std::string& name);  // a client name; nothing: it stays

class Collector {
public:
    // Whether the source's file at path is to be read and handed to take(): a palette the server lets be worn, the
    // first of its name (the names compared without case, as the client looks them up). Counted either way.
    bool wants(const std::string& path);
    void take(const std::string& path, std::vector<uint8_t> bytes);  // empty: it stays out
    size_t kept() const { return palettes_.size(); }
    size_t hair() const;
    uint32_t found() const { return found_; }  // the palettes looked at
    const std::array<uint32_t, kLeftKinds>& left() const { return left_; }
    // The GRF 0x200 of the palettes kept, each different one's data once - entries of the same palette share it
    // (uaRO.exe reads an entry by its own offset and sizes, FUN_00510fd0) -, zlib, aligned to 8 as GRF tools write it.
    // blocks: the different ones.
    std::vector<uint8_t> grf(uint32_t& blocks) const;

private:
    std::vector<std::pair<std::string, std::vector<uint8_t>>> palettes_;  // client names
    std::unordered_set<std::string> names_;                             // in lower case
    std::array<uint32_t, kLeftKinds> left_{};
    uint32_t found_ = 0;
};

}  // namespace uagfx::palettes
