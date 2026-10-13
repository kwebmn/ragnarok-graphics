#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/states.hpp"

namespace uagfx {

// The pipeline state a draw needs; packed, it is the key the renderer caches pipelines by. The keys met in a session
// are saved and their pipelines built at the next start, so an effect does not stutter the first time it is drawn
// (1B review M8).
struct PipelineKey {
    bool zEnable = false;
    bool zWrite = false;
    uint8_t zFunc = 0;     // d3d7::CMP_*
    uint8_t cull = 0;      // d3d7::CULL_*
    bool blend = false;
    uint8_t srcBlend = 0;  // d3d7::BLEND_*
    uint8_t dstBlend = 0;
    bool twoUv = false;    // FVF_TL2: a second UV set
    bool operator==(const PipelineKey& o) const;
};

PipelineKey pipelineKey(const DrawSetup& s, uint32_t fvf);
uint64_t packPipelineKey(const PipelineKey& k);
PipelineKey unpackPipelineKey(uint64_t v);

// The saved keys: pipelineKeysHeader(), then pipelineKeyLine() per key. Lines that are not hex are skipped; a file
// without this header (another version, or none) gives nothing.
std::vector<uint64_t> parsePipelineKeys(const std::string& text);
std::string pipelineKeysHeader();
std::string pipelineKeyLine(uint64_t v);

}  // namespace uagfx
