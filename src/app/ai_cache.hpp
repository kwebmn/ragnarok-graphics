#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/ai/cache_index.hpp"

namespace uagfx::app {

// upscaled.grf next to uaRO.exe for the Ready textures page: its table only (the pictures stay on disk).
struct AiCacheFile {
    bool exists = false;
    uint64_t bytes = 0;  // the file's size
    std::vector<ai::CacheEntry> entries;
    std::string error;  // when it is there but cannot be read
};
AiCacheFile readAiCache(const std::wstring& path);

// The file again with only the entries `keep` names (their data copied as stored, through "<path>.new" moved over
// it); removed when nothing stays. "" or what went wrong, in English (it goes into a message after a translated
// sentence).
std::string rewriteAiCache(const std::wstring& path, const std::vector<std::string>& keep);
std::string removeAiCache(const std::wstring& path);

// A running uaro-render64.exe writes upscaled.grf: it holds its writer mutex (ai_textures.cpp).
bool aiCacheInUse();

}  // namespace uagfx::app
