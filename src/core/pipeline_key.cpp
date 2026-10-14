#include "core/pipeline_key.hpp"

#include <cstdio>
#include <set>
#include <sstream>

#include "core/d3d7.hpp"

namespace uagfx {

bool PipelineKey::operator==(const PipelineKey& o) const {
    return zEnable == o.zEnable && zWrite == o.zWrite && zFunc == o.zFunc && cull == o.cull && blend == o.blend &&
           srcBlend == o.srcBlend && dstBlend == o.dstBlend && twoUv == o.twoUv && samples == o.samples &&
           alphaToCoverage == o.alphaToCoverage;
}

PipelineKey pipelineKey(const DrawSetup& s, uint32_t fvf) {
    PipelineKey k;
    k.zEnable = s.zEnable;
    k.zWrite = s.zWrite;
    k.zFunc = uint8_t(s.zFunc & 15);
    k.cull = uint8_t(s.cull & 3);
    k.blend = s.blend;
    k.srcBlend = uint8_t(s.srcBlend & 15);
    k.dstBlend = uint8_t(s.dstBlend & 15);
    k.twoUv = fvf == d3d7::FVF_TL2;
    return k;
}

namespace {

// 1, 2, 4, 8 samples as 0-3; a key from before stage 4 has 0 there: one sample.
uint64_t samplesCode(uint8_t n) { return n >= 8 ? 3 : n >= 4 ? 2 : n >= 2 ? 1 : 0; }

}  // namespace

uint64_t packPipelineKey(const PipelineKey& k) {
    return uint64_t(k.zEnable) | uint64_t(k.zWrite) << 1 | uint64_t(k.zFunc & 15) << 2 | uint64_t(k.cull & 3) << 6 |
           uint64_t(k.blend) << 8 | uint64_t(k.srcBlend & 15) << 9 | uint64_t(k.dstBlend & 15) << 13 |
           uint64_t(k.twoUv) << 17 | samplesCode(k.samples) << 18 | uint64_t(k.alphaToCoverage) << 20;
}

PipelineKey unpackPipelineKey(uint64_t v) {
    PipelineKey k;
    k.zEnable = (v & 1) != 0;
    k.zWrite = (v >> 1 & 1) != 0;
    k.zFunc = uint8_t(v >> 2 & 15);
    k.cull = uint8_t(v >> 6 & 3);
    k.blend = (v >> 8 & 1) != 0;
    k.srcBlend = uint8_t(v >> 9 & 15);
    k.dstBlend = uint8_t(v >> 13 & 15);
    k.twoUv = (v >> 17 & 1) != 0;
    k.samples = uint8_t(1u << (v >> 18 & 3));
    k.alphaToCoverage = (v >> 20 & 1) != 0;
    return k;
}

std::string pipelineKeysHeader() { return "uaro-graphics pipelines 1\n"; }

std::string pipelineKeyLine(uint64_t v) {
    char line[24];
    std::snprintf(line, sizeof line, "%llx\n", static_cast<unsigned long long>(v));
    return line;
}

std::vector<uint64_t> parsePipelineKeys(const std::string& text) {
    std::vector<uint64_t> keys;
    std::istringstream in(text);
    std::string line;
    bool header = false;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (!header) {
            if (line + "\n" != pipelineKeysHeader()) return keys;
            header = true;
            continue;
        }
        if (line.empty() || line.size() > 16 || line.find_first_not_of("0123456789abcdef") != std::string::npos)
            continue;
        const uint64_t v = std::stoull(line, nullptr, 16);
        if (validPipelineKey(v)) keys.push_back(v);
    }
    return keys;
}

bool validPipelineKey(uint64_t v) {
    const PipelineKey k = unpackPipelineKey(v);
    if (packPipelineKey(k) != v || k.cull < d3d7::CULL_NONE) return false;
    if (k.alphaToCoverage && (k.samples < 2 || k.blend)) return false;
    if (k.zEnable ? k.zFunc < d3d7::CMP_NEVER || k.zFunc > d3d7::CMP_ALWAYS : k.zFunc != d3d7::CMP_ALWAYS || k.zWrite)
        return false;
    if (!k.blend) return k.srcBlend == d3d7::BLEND_ONE && k.dstBlend == d3d7::BLEND_ZERO;
    const auto factor = [](uint8_t f) { return f >= d3d7::BLEND_ZERO && f <= d3d7::BLEND_SRCALPHASAT; };
    return factor(k.srcBlend) && factor(k.dstBlend);
}

std::string pipelineKeysText(const std::vector<uint64_t>& keys) {
    std::string text = pipelineKeysHeader();
    for (const uint64_t k : std::set<uint64_t>(keys.begin(), keys.end())) text += pipelineKeyLine(k);
    return text;
}

}  // namespace uagfx
