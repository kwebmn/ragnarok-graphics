#include "core/portal_capture.hpp"

#include <cstdio>

#include "core/report_upload.hpp"

namespace uagfx::portal_capture {

bool Recorder::take(const std::string& map) {
    if (map != map_ && left_ == 0 && maps_ < 3) {  // a new map: its two seconds begin
        map_ = map;
        ++maps_;
        left_ = 120;
        frame_ = 0;
    }
    return map == map_ && left_ > 0;
}

void Recorder::endFrame() {
    if (left_ <= 0) return;
    --left_;
    ++frame_;
}

std::vector<std::string> drawLines(int frame, int effectId, const DrawSetup& s, uint32_t texture, int width,
                                   int height, const Vertex* v, int count, const uint16_t* indices, int indexCount) {
    std::vector<std::string> out;
    char line[320];
    std::snprintf(line, sizeof line,
                  "portal capture: frame %d effect %d %s blend %d/%d %s z %d/%d alpha test %d ref %d stage0 %d/%d "
                  "texture %u %dx%d, %d vertices, %d indices",
                  frame, effectId, s.ui ? "interface" : "world", int(s.srcBlend), int(s.dstBlend),
                  s.blend ? "on" : "off",
                  s.zEnable ? 1 : 0, s.zWrite ? 1 : 0, s.alphaTest ? 1 : 0, int(s.alphaRef), int(s.stage[0].colorOp),
                  int(s.stage[0].alphaOp), texture, width, height, count, indexCount);
    out.push_back(line);
    for (int i = 0; i < count; ++i) {
        std::snprintf(line, sizeof line, "  v %.1f %.1f %.6f %.6f %08X %08X %.3f %.3f", v[i].x, v[i].y, v[i].z,
                      v[i].rhw, unsigned(v[i].diffuse), unsigned(v[i].specular), v[i].u, v[i].v);
        out.push_back(line);
    }
    std::string ix;
    for (int i = 0; i < indexCount && indices; ++i) ix += (i ? " " : "") + std::to_string(indices[i]);
    if (!ix.empty()) out.push_back("  i " + ix);
    return out;
}

std::vector<std::string> textureLines(uint32_t texture, int width, int height, const uint32_t* argb) {
    std::vector<std::string> out;
    char head[96];
    std::snprintf(head, sizeof head, "portal capture: texture %u %dx%d (ARGB, base64)", texture, width, height);
    out.push_back(head);
    if (!argb || width <= 0 || height <= 0) return out;
    std::string bytes(reinterpret_cast<const char*>(argb), size_t(width) * size_t(height) * 4);
    const std::string text = report::base64(bytes);
    for (size_t i = 0; i < text.size(); i += 160) out.push_back("  t " + text.substr(i, 160));
    return out;
}

}  // namespace uagfx::portal_capture
