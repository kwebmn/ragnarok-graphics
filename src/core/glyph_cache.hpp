#pragma once
#include <cstddef>
#include <cstdint>
#include <list>
#include <unordered_map>

#include "core/twin_text.hpp"

namespace uagfx {

// Stage 6 (spec §4): letters at the twin's size, the least recently used ones dropped when they no longer fit the
// budget. font: the driver's number for a game font at the twin's size; ch: the letter. A pointer or reference it
// returns stays valid until the next put or clear.
class GlyphCache {
public:
    explicit GlyphCache(size_t budgetBytes) : budget_(budgetBytes) {}
    const GlyphBitmap* find(uint32_t font, uint32_t ch);              // makes it the most recent
    const GlyphBitmap& put(uint32_t font, uint32_t ch, GlyphBitmap g);  // the new letter itself is never dropped
    void clear();
    size_t size() const { return map_.size(); }
    size_t bytes() const { return bytes_; }
    static size_t cost(const GlyphBitmap& g);

private:
    struct Entry {
        uint64_t key;
        GlyphBitmap glyph;
        size_t bytes;
    };
    size_t budget_;
    size_t bytes_ = 0;
    std::list<Entry> order_;  // the most recent first
    std::unordered_map<uint64_t, std::list<Entry>::iterator> map_;
};

}  // namespace uagfx
