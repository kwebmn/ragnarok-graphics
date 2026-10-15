#pragma once
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "core/texture_pack.hpp"

namespace uagfx::pack {

// 0.53: the texture pack from the site (uaro.kiev.ua) into the player's uaro-textures.zip - the settings app reads
// the remote pack's central directory by HTTP ranges and fetches only the pictures the local pack has not: a download
// cut short goes on where it stopped, a newer pack on the site brings only its new pictures.
constexpr const char* kPackUrl = "https://uaro.kiev.ua/files/graphics/uaro-textures.zip";

// A run of the remote file fetched by one request: the bytes [from, to) holding these entries (indexes into the
// remote list), each with its local header.
struct Span {
    uint64_t from = 0, to = 0;
    std::vector<size_t> entries;
};

// The spans for the stored pictures `have` says the local pack lacks (the index aside: it is merged), in the file's
// order; a gap of at most `gapMost` bytes is fetched along rather than asked for apart, a span holds at most
// `spanMost` bytes (one picture more when it is bigger). end: where the pictures stop (the central directory).
std::vector<Span> downloadSpans(const std::vector<ZipEntry>& remote,
                                const std::function<bool(const std::string&)>& have,
                                uint64_t end, uint64_t gapMost = 256u << 10, uint64_t spanMost = 16u << 20);
// An entry's data inside a fetched span: its local header checked, its bytes found and their CRC-32 checked.
bool entryData(const uint8_t* span, size_t spanSize, uint64_t spanFrom, const ZipEntry& e, const uint8_t*& data,
               std::string& error);
// The local index and the site's together: the site's pictures and aliases, and the local ones it has not (pictures
// the player's own game made, [ai] generate).
std::string mergedIndex(const std::string& local, const std::string& remote);

}  // namespace uagfx::pack
