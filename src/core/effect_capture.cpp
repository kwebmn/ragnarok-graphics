#include "core/effect_capture.hpp"

#include <algorithm>
#include <cstdio>

#include "core/report_upload.hpp"

namespace uagfx::effect_capture {

int Recorder::offset(int id, uint32_t frame) const {
    const auto it = effects_.find(id);
    return it == effects_.end() ? -1 : int(frame - it->second.first);
}

bool Recorder::take(int id, uint32_t frame) {
    if (full()) return false;
    auto it = effects_.find(id);
    if (it == effects_.end()) {
        if (int(effects_.size()) >= kMostEffects) return false;
        it = effects_.emplace(id, Effect{frame, frame, 0}).first;
    }
    Effect& e = it->second;
    const int at = int(frame - e.first);
    if (std::find(std::begin(kFrames), std::end(kFrames), at) == std::end(kFrames)) return false;
    if (e.frame != frame) {
        e.frame = frame;
        e.draws = 0;
    }
    return e.draws++ < kMostDrawsAFrame;
}

bool Recorder::textureTold(uint32_t id) {
    if (std::find(textures_.begin(), textures_.end(), id) != textures_.end()) return false;
    textures_.push_back(id);
    return true;
}

std::string drawText(int id, int offset, const DrawSetup& s, uint32_t texture, int width, int height,
                     const Vertex* v, int count, const uint16_t* indices, int indexCount) {
    std::string out;
    char line[320];
    std::snprintf(line, sizeof line,
                  "effect %d +%d %s blend %d/%d %s z %d/%d alpha test %d ref %d stage0 %d/%d texture %u %dx%d, "
                  "%d vertices, %d indices\n",
                  id, offset, s.ui ? "interface" : "world", int(s.srcBlend), int(s.dstBlend), s.blend ? "on" : "off",
                  s.zEnable ? 1 : 0, s.zWrite ? 1 : 0, s.alphaTest ? 1 : 0, int(s.alphaRef),
                  int(s.stage[0].colorOp), int(s.stage[0].alphaOp), texture, width, height, count, indexCount);
    out += line;
    for (int i = 0; i < count; ++i) {
        std::snprintf(line, sizeof line, " v %.1f %.1f %.5f %.6f %08X %08X %.3f %.3f\n", v[i].x, v[i].y, v[i].z,
                      v[i].rhw, unsigned(v[i].diffuse), unsigned(v[i].specular), v[i].u, v[i].v);
        out += line;
    }
    if (indices && indexCount > 0) {
        out += " i";
        for (int i = 0; i < indexCount; ++i) out += ' ' + std::to_string(indices[i]);
        out += '\n';
    }
    return out;
}

std::string firstText(int id, int skill) {
    return "first " + std::to_string(id) + " after skill " + std::to_string(skill) + "\n";
}

std::string textureText(uint32_t texture, int width, int height, const uint32_t* argb) {
    if (!argb || width <= 0 || height <= 0) return {};
    const int w = std::min(width, kTextureSide), h = std::min(height, kTextureSide);
    std::string bytes(size_t(w) * size_t(h) * 4, '\0');
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const int x0 = x * width / w, x1 = std::max(x0 + 1, (x + 1) * width / w);
            const int y0 = y * height / h, y1 = std::max(y0 + 1, (y + 1) * height / h);
            uint32_t sum[4] = {0, 0, 0, 0}, n = 0;
            for (int yy = y0; yy < y1; ++yy)
                for (int xx = x0; xx < x1; ++xx, ++n) {
                    const uint32_t c = argb[size_t(yy) * size_t(width) + size_t(xx)];
                    for (int k = 0; k < 4; ++k) sum[k] += (c >> (8 * k)) & 255;
                }
            for (int k = 0; k < 4; ++k) bytes[(size_t(y) * size_t(w) + size_t(x)) * 4 + size_t(k)] = char(sum[k] / n);
        }
    char head[96];
    std::snprintf(head, sizeof head, "texture %u %dx%d as %dx%d ARGB base64\n", texture, width, height, w, h);
    return head + (" t " + report::base64(bytes)) + "\n";
}

}  // namespace uagfx::effect_capture
