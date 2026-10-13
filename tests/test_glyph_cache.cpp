#include "core/glyph_cache.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
GlyphBitmap letter(int bytes) {
    GlyphBitmap g;
    g.width = bytes;
    g.height = 1;
    g.coverage.assign(size_t(bytes), 64);
    return g;
}
}  // namespace

TEST(glyph_cache_finds_what_was_put) {
    GlyphCache c(1 << 20);
    CHECK(c.find(1, L'a') == nullptr);
    c.put(1, L'a', letter(10));
    const GlyphBitmap* g = c.find(1, L'a');
    CHECK(g != nullptr && g->width == 10);
    CHECK(c.find(2, L'a') == nullptr);  // another font
    CHECK(c.find(1, L'b') == nullptr);
    CHECK_EQ(c.size(), size_t(1));
}

TEST(glyph_cache_drops_the_least_recent_past_its_budget) {
    const size_t one = GlyphCache::cost(letter(100));
    GlyphCache c(one * 2);
    c.put(1, 'a', letter(100));
    c.put(1, 'b', letter(100));
    CHECK(c.find(1, 'a') != nullptr);  // 'a' is now the most recent
    c.put(1, 'c', letter(100));
    CHECK(c.find(1, 'a') != nullptr);
    CHECK(c.find(1, 'b') == nullptr);
    CHECK(c.find(1, 'c') != nullptr);
    CHECK(c.bytes() <= one * 2);
}

TEST(glyph_cache_keeps_a_new_letter_bigger_than_its_budget) {
    GlyphCache c(10);
    const GlyphBitmap& g = c.put(1, 'a', letter(1000));
    CHECK_EQ(g.width, 1000);
    CHECK(c.find(1, 'a') != nullptr);
}

TEST(glyph_cache_put_again_replaces) {
    GlyphCache c(1 << 20);
    c.put(1, 'a', letter(10));
    c.put(1, 'a', letter(20));
    CHECK_EQ(c.size(), size_t(1));
    CHECK_EQ(c.bytes(), GlyphCache::cost(letter(20)));
    CHECK_EQ(c.find(1, 'a')->width, 20);
    c.clear();
    CHECK_EQ(c.size(), size_t(0));
    CHECK_EQ(c.bytes(), size_t(0));
    CHECK(c.find(1, 'a') == nullptr);
}
