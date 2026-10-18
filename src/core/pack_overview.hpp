#pragma once
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/texture_pack.hpp"

namespace uagfx::pack {

// 2.3, uaRO Studio's Pack tab (P1): the pack in numbers - its pictures by group, model and kind, the orphans (no
// texture names them) and the missing (textures without one), its size, and the bytes no entry points at any more
// (pictures replaced or taken out: Compact gives them back).
struct Overview {
    std::map<std::string, uint32_t> byGroup, byModel, byKind;  // "?" when the index does not say
    uint32_t pictures = 0, orphans = 0, missing = 0;
    uint64_t bytes = 0, unread = 0;
};
Overview overviewOf(const std::vector<ZipEntry>& entries, uint64_t fileSize, const Index& index, uint32_t orphans,
                    uint32_t missing);

}  // namespace uagfx::pack
