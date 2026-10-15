#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/ai/cache_index.hpp"

namespace uagfx::app {

// 0.48: the texture pack uaro-textures.zip next to uaRO.exe for the Ready textures page - its pictures with their
// sizes, kinds and maps (its index); upscaled.grf (0.11-0.47) only to be deleted.
struct PackFile {
    bool exists = false;
    uint64_t bytes = 0;  // the file's size
    std::vector<ai::CacheEntry> entries;
    std::string error;  // when it is there but cannot be read
};
PackFile readPack(const std::wstring& path);

// The pack again with only the pictures `keep` names (through "<path>.new" moved over it); removed when none stays.
// "" or what went wrong, in English (it goes into a message after a translated sentence).
std::string rewritePack(const std::wstring& path, const std::vector<std::string>& keep);
std::string removeFile(const std::wstring& path);
uint64_t fileBytes(const std::wstring& path);  // 0: not there

// Its builder (uaro-render64.exe --pack) or a game making pictures writes the pack: it holds pack::kWriterMutex.
bool packInUse();

}  // namespace uagfx::app
