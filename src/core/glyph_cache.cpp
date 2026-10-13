#include "core/glyph_cache.hpp"

#include <utility>

namespace uagfx {

size_t GlyphCache::cost(const GlyphBitmap& g) { return g.coverage.size() + sizeof(Entry) + 32; }

const GlyphBitmap* GlyphCache::find(uint32_t font, uint32_t ch) {
    const auto it = map_.find(uint64_t(font) << 32 | ch);
    if (it == map_.end()) return nullptr;
    order_.splice(order_.begin(), order_, it->second);
    return &it->second->glyph;
}

const GlyphBitmap& GlyphCache::put(uint32_t font, uint32_t ch, GlyphBitmap g) {
    const uint64_t key = uint64_t(font) << 32 | ch;
    const auto old = map_.find(key);
    if (old != map_.end()) {
        bytes_ -= old->second->bytes;
        order_.erase(old->second);
        map_.erase(old);
    }
    const size_t c = cost(g);
    order_.push_front(Entry{key, std::move(g), c});
    try {
        map_[key] = order_.begin();
    } catch (...) {  // no memory for its index: the letter goes again, the cache stays as it was (0.5.2 review 2c)
        order_.pop_front();
        throw;
    }
    bytes_ += c;
    while (bytes_ > budget_ && order_.size() > 1) {
        bytes_ -= order_.back().bytes;
        map_.erase(order_.back().key);
        order_.pop_back();
    }
    return order_.front().glyph;
}

void GlyphCache::clear() {
    order_.clear();
    map_.clear();
    bytes_ = 0;
}

}  // namespace uagfx
