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
    uint8_t samples = 1;   // stage 4: of the target - 1, 2, 4 or 8
    bool alphaToCoverage = false;  // stage 4: alpha testing as coverage (several samples, no blending)
    bool layer = false;  // part 3 (DLSS): the interface layer - alpha kept for the composite, no depth
    bool glow = false;   // 0.18: a world draw that also writes the light it adds into the glow target (two targets)
    bool water = false;  // 0.19: a cell of a map's water, drawn by the modern water's shaders
    bool mask = false;   // 0.22.3: a character or an effect in the DLSS world - its coverage into the alpha
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

// A key the renderer makes (0.4.1 and 0.4.2 reviews): only the packed fields, in resolveDrawSetup's forms - no Z test
// means ALWAYS and no Z writes, no blending means ONE and ZERO, blend factors 1-11 (BOTH* resolved), a cull mode;
// alpha to coverage only with several samples and no blending; the interface layer with one sample and no Z test;
// the glow's second target, the water and the DLSS mask never with the layer.
// Damaged lines - two game instances appending at once - are skipped by parsePipelineKeys.
bool validPipelineKey(uint64_t v);
// The file's text: the header, then each key once, in increasing order.
std::string pipelineKeysText(const std::vector<uint64_t>& keys);

}  // namespace uagfx
