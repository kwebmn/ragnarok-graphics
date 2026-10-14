#include "core/texture_levels.hpp"

#include <algorithm>

namespace uagfx {
namespace {

Rect unite(const Rect& a, const Rect& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return Rect{std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right),
                std::max(a.bottom, b.bottom)};
}

}  // namespace

TextureLevels::TextureLevels(int width, int height, PixelFormat format, int levels) {
    for (int i = 0; i < std::max(1, levels); ++i) {
        images_.emplace_back(std::max(1, width >> i), std::max(1, height >> i), format);
        dirty_.push_back(images_.back().bounds());
    }
}

void TextureLevels::setKey(const ColorKey& key) {
    key_ = key;
    for (size_t i = 0; i < images_.size(); ++i) dirty_[i] = images_[i].bounds();
}

void TextureLevels::changed(int level, Rect r) {
    if (level < 0 || level >= levels() || !clipRect(images_[size_t(level)].bounds(), r)) return;
    dirty_[size_t(level)] = unite(dirty_[size_t(level)], r);
}

void TextureLevels::slotWritten(Rect r) {
    if (levels() == 0 || !clipRect(images_[0].bounds(), r)) return;
    for (const Rect& s : slots_)
        if (s == r) return;  // written again before the upload: the cell's newest frame goes once
    slots_.push_back(r);
}

bool TextureLevels::pending() const {
    for (const Rect& r : dirty_)
        if (!r.empty()) return true;
    return false;
}

void TextureLevels::sync(FrameRecord& record, uint32_t texture) {
    std::vector<uint32_t> argb;
    for (size_t i = 0; i < images_.size(); ++i) {
        if (dirty_[i].empty()) continue;
        toArgbRect(images_[i], dirty_[i], key_, argb);
        record.upload(texture, uint32_t(i), dirty_[i], argb.data(), i == 0 ? slots_ : std::vector<Rect>());
        dirty_[i] = Rect{};
        if (i == 0) slots_.clear();
    }
}

}  // namespace uagfx
