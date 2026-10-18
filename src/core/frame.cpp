#include "core/frame.hpp"

#include <cstring>

#include "core/d3d7.hpp"

namespace uagfx {

uint32_t FrameRecord::stride(uint32_t fvf) { return fvf == d3d7::FVF_TL2 ? 40u : 32u; }

uint32_t FrameRecord::addState(const DrawSetup& s) {
    if (!states_.empty() && states_.back() == s) return uint32_t(states_.size() - 1);
    states_.push_back(s);
    return uint32_t(states_.size() - 1);
}

// Every call adds its data, then its command: memory running out between the two leaves data no command uses, never
// a command without its data (0.5.2 review 2a).
void FrameRecord::clear(const ClearCmd& c) {
    clears_.push_back(c);
    commands_.push_back({Cmd::Clear, uint32_t(clears_.size() - 1)});
}

void FrameRecord::viewport(const ViewportCmd& v) {
    viewports_.push_back(v);
    commands_.push_back({Cmd::Viewport, uint32_t(viewports_.size() - 1)});
}

void FrameRecord::draw(DrawCmd cmd, const void* vertices, uint32_t vertexCount, const uint16_t* indices,
                       uint32_t indexCount) {
    const uint32_t bytes = vertexCount * stride(cmd.fvf);
    cmd.vertexOffset = uint32_t(vertices_.size());
    cmd.vertexCount = vertexCount;
    vertices_.resize(vertices_.size() + bytes);
    std::memcpy(vertices_.data() + cmd.vertexOffset, vertices, bytes);
    cmd.firstIndex = uint32_t(indices_.size());
    cmd.indexCount = indices ? indexCount : 0;
    if (indices) indices_.insert(indices_.end(), indices, indices + indexCount);
    draws_.push_back(cmd);
    commands_.push_back({Cmd::Draw, uint32_t(draws_.size() - 1)});
}

uint32_t FrameRecord::addPixels(const uint32_t* argb, size_t count) {
    const uint32_t at = uint32_t(pixels_.size());
    pixels_.insert(pixels_.end(), argb, argb + count);
    return at;
}

void FrameRecord::upload(uint32_t texture, uint32_t level, const Rect& rect, const uint32_t* argb,
                         const std::vector<SpriteCell>& slots) {
    UploadCmd u;
    u.texture = texture;
    u.level = level;
    u.rect = rect;
    u.pixels = addPixels(argb, size_t(rect.width()) * size_t(rect.height()));
    u.firstSlot = uint32_t(slots_.size());
    u.slotCount = uint32_t(slots.size());
    slots_.insert(slots_.end(), slots.begin(), slots.end());
    uploads_.push_back(u);
    commands_.push_back({Cmd::Upload, uint32_t(uploads_.size() - 1)});
}

void FrameRecord::writeTarget(const Rect& rect, const uint32_t* argb) {
    WriteTargetCmd w;
    w.rect = rect;
    w.pixels = addPixels(argb, size_t(rect.width()) * size_t(rect.height()));
    writes_.push_back(w);
    commands_.push_back({Cmd::WriteTarget, uint32_t(writes_.size() - 1)});
}

void FrameRecord::reset() {
    commands_.clear();
    clears_.clear();
    viewports_.clear();
    draws_.clear();
    uploads_.clear();
    writes_.clear();
    states_.clear();
    vertices_.clear();
    indices_.clear();
    pixels_.clear();
    slots_.clear();
}

uint8_t spriteSourceAt(const std::vector<SpriteCell>& cells, int width, int height, float u, float v) {
    const int x = int(u * float(width)), y = int(v * float(height));
    for (const SpriteCell& c : cells)
        if (x >= c.rect.left && x < c.rect.right && y >= c.rect.top && y < c.rect.bottom) return c.source;
    return 0;
}

uint8_t spriteSourceOf(const std::vector<SpriteCell>& cells, int width, int height, const std::vector<uint8_t>& bytes,
                       size_t offset, size_t stride, uint32_t count) {
    if (cells.empty() || !count || stride < 32 || offset + size_t(count) * stride > bytes.size()) return 0;
    float u = 0.0f, v = 0.0f;
    for (uint32_t i = 0; i < count; ++i) {
        float uv[2];
        std::memcpy(uv, bytes.data() + offset + size_t(i) * stride + 24, sizeof uv);
        u += uv[0];
        v += uv[1];
    }
    return spriteSourceAt(cells, width, height, u / float(count), v / float(count));
}

}  // namespace uagfx
