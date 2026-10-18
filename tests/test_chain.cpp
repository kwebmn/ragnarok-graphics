#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "core/ai/chain.hpp"
#include "core/ai/picture_metrics.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

uint32_t grey(int v, int a = 255) {
    v = v < 0 ? 0 : v > 255 ? 255 : v;
    return uint32_t(a) << 24 | uint32_t(v) << 16 | uint32_t(v) << 8 | uint32_t(v);
}
int red(uint32_t p) { return int(p >> 16 & 0xFF); }
int blue(uint32_t p) { return int(p & 0xFF); }

std::vector<uint32_t> noise(int w, int h) {
    std::vector<uint32_t> p(size_t(w) * size_t(h));
    uint32_t s = 2166136261u;
    for (uint32_t& v : p) {
        s = (s ^ 0x9E3779B9u) * 16777619u;
        v = grey(64 + int(s >> 24) / 2);
    }
    return p;
}

std::vector<uint32_t> nearest4(const std::vector<uint32_t>& p, int w, int h) {
    std::vector<uint32_t> out(size_t(w) * 4 * size_t(h) * 4);
    for (int y = 0; y < h * 4; ++y)
        for (int x = 0; x < w * 4; ++x) out[size_t(y) * size_t(w * 4) + size_t(x)] = p[size_t(y / 4 * w + x / 4)];
    return out;
}

// bilinear x4 (between texel centres, edges clamped): a smooth picture
std::vector<uint32_t> smooth4(const std::vector<uint32_t>& p, int w, int h) {
    std::vector<uint32_t> out(size_t(w) * 4 * size_t(h) * 4);
    for (int y = 0; y < h * 4; ++y)
        for (int x = 0; x < w * 4; ++x) {
            const float fx = std::fmax(0.0f, (float(x) + 0.5f) / 4.0f - 0.5f);
            const float fy = std::fmax(0.0f, (float(y) + 0.5f) / 4.0f - 0.5f);
            const int x0 = std::min(int(fx), w - 1), y0 = std::min(int(fy), h - 1);
            const int x1 = std::min(x0 + 1, w - 1), y1 = std::min(y0 + 1, h - 1);
            const float ax = fx - float(x0), ay = fy - float(y0);
            const auto l = [&](int xx, int yy) { return float(p[size_t(yy * w + xx)] & 0xFF); };
            const float top = l(x0, y0) * (1 - ax) + l(x1, y0) * ax, bottom = l(x0, y1) * (1 - ax) + l(x1, y1) * ax;
            const float v = top * (1 - ay) + bottom * ay;
            out[size_t(y) * size_t(w * 4) + size_t(x)] = grey(int(v + 0.5f));
        }
    return out;
}

}  // namespace

TEST(chain_text_round_trips) {
    // 2.3 (T5): the chain as the pack's '+' line and the studio's rules write it
    ai::Chain c;
    CHECK_EQ(ai::chainText(c), std::string("general x4"));
    c.model = "anime";
    c.scale = 2;
    c.deblock = c.repairKey = c.tiles = c.matchTone = c.cleanAlpha = true;
    c.grain = 0.3f;
    c.sharpen = 0.4f;
    c.blendModel = "general";
    c.original = 0.2f;
    c.denoise = 50;
    const std::string text = ai::chainText(c);
    CHECK_EQ(text, std::string("anime x2 deblock key tiles tone grain=0.30 sharpen=0.40 alpha blend=general "
                               "original=0.20 denoise=50"));
    ai::Chain back;
    CHECK(ai::parseChain(text, back));
    CHECK_EQ(ai::chainText(back), text);
    CHECK(!ai::parseChain("anime x3", back));
    CHECK(!ai::parseChain("anime x4 sparkle", back));
    CHECK(!ai::parseChain("", back));
    CHECK(ai::parseChain("anime6b", back) && back.scale == 4);
}

TEST(chain_deblock_only_where_flat) {
    // a JPEG's 8x8 blocks: flat, a few levels apart at their edges - smoothed; a real edge there stays
    std::vector<uint32_t> p(32 * 8);
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 32; ++x) p[size_t(y * 32 + x)] = grey(x < 8 ? 100 : x < 16 ? 106 : x < 24 ? 200 : 200);
    std::vector<uint32_t> d = p;
    ai::deblock(d, 32, 8);
    CHECK(std::abs(red(d[size_t(3 * 32 + 8)]) - red(d[size_t(3 * 32 + 7)])) < 6);  // the blocks' step eased
    CHECK_EQ(red(d[size_t(3 * 32 + 16)]), 200);  // a real edge (94 levels) kept
    CHECK_EQ(red(d[size_t(3 * 32 + 15)]), 106);
    CHECK_EQ(red(d[size_t(3 * 32 + 2)]), 100);  // inside a block: as it was
}

TEST(chain_repair_key_takes_near_magenta) {
    std::vector<uint32_t> p = {0xFFFA0AF5u, 0xFFC864C8u, 0xFF00FF00u, 0xFFFF00FFu};
    ai::repairKey(p, 4, 1);
    CHECK_EQ(p[0], 0x00FF00FFu);  // a JPEG's magenta: the key
    CHECK_EQ(p[1], 0xFFC864C8u);  // a pink of the picture: kept
    CHECK_EQ(p[2], 0xFF00FF00u);
    CHECK_EQ(p[3], 0x00FF00FFu);
}

TEST(chain_match_tone_moves_the_mean) {
    const std::vector<uint32_t> o = noise(32, 32);
    std::vector<uint32_t> p = nearest4(o, 32, 32);
    for (uint32_t& v : p) v = (v & 0xFF00FFFFu) | uint32_t(std::min(255, red(v) + 30)) << 16;
    CHECK(ai::measure(o.data(), 32, 32, p.data(), 128, 128, true).tone > 0.04f);
    ai::matchTone(p, 128, 128, o.data(), 32, 32);
    CHECK(ai::measure(o.data(), 32, 32, p.data(), 128, 128, true).tone < 0.01f);
}

TEST(chain_restore_grain_adds_detail) {
    const std::vector<uint32_t> o = noise(32, 32);
    std::vector<uint32_t> p = smooth4(o, 32, 32);
    const float before = ai::measure(o.data(), 32, 32, p.data(), 128, 128, true).detail;
    ai::restoreGrain(p, 128, 128, o.data(), 32, 32, 1.0f);
    CHECK(ai::measure(o.data(), 32, 32, p.data(), 128, 128, true).detail > before * 1.3f);
}

TEST(chain_sharpen_raises_edges_not_flats) {
    // a soft edge (100, 104, 116, 120) gets steeper; a flat stays (contrast adaptive: never past its neighbours)
    std::vector<uint32_t> p(16 * 16);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) p[size_t(y * 16 + x)] = grey(x < 6 ? 100 : x == 6 ? 104 : x == 7 ? 116 : 120);
    ai::sharpenAdaptive(p, 16, 16, 0.5f);
    CHECK(red(p[size_t(8 * 16 + 6)]) < 104);
    CHECK(red(p[size_t(8 * 16 + 7)]) > 116);
    CHECK_EQ(red(p[size_t(8 * 16 + 2)]), 100);
    CHECK_EQ(red(p[size_t(8 * 16 + 12)]), 120);
}

TEST(chain_clean_alpha_follows_the_key) {
    // a sprite on the key; the network's picture opaque all over, pink beside where the key was
    std::vector<uint32_t> o(16 * 16);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x) o[size_t(y * 16 + x)] = x >= 4 && x < 12 ? 0xFF30A030u : 0x00FF00FFu;
    std::vector<uint32_t> p = nearest4(o, 16, 16);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            uint32_t& v = p[size_t(y * 64 + x)];
            v |= 0xFF000000u;
            if (x == 16 || x == 47) v = 0xFFD050C0u;  // the rim gone pink
        }
    ai::cleanAlpha(p, 64, 64, o.data(), 16, 16);
    const ai::Metrics m = ai::measure(o.data(), 16, 16, p.data(), 64, 64, false);
    CHECK(m.alpha < 0.01f);
    CHECK(m.fringe < 0.01f);
    CHECK_EQ(p[size_t(5 * 64 + 2)] >> 24, 0u);  // where the key is: transparent again
    CHECK_EQ(p[size_t(5 * 64 + 30)] >> 24, 255u);
}

TEST(chain_blend_by_edges_takes_the_other_on_edges) {
    std::vector<uint32_t> o(32 * 32);
    for (int y = 0; y < 32; ++y)
        for (int x = 0; x < 32; ++x) o[size_t(y * 32 + x)] = grey(x < 16 ? 40 : 220);
    std::vector<uint32_t> a(128 * 128, grey(50));
    const std::vector<uint32_t> b(128 * 128, grey(200));
    ai::blendByEdges(a, b, 128, 128, o.data(), 32, 32);
    CHECK(red(a[size_t(60 * 128 + 64)]) > 150);  // on the original's edge: mostly the other
    CHECK(red(a[size_t(60 * 128 + 10)]) < 60);   // flat: its own
}

TEST(chain_tiles_ask_the_network_per_tile) {
    // T6: an atlas's tiles each through the network on its own, its edges wrapped about it
    const std::vector<uint32_t> o = noise(64, 64);
    int calls = 0;
    bool sizes = true;
    const ai::Enlarge fake = [&](const std::string& model, int, const std::vector<uint32_t>& in, int w, int h,
                                 std::vector<uint32_t>& out, std::string&) {
        ++calls;
        sizes = sizes && w == 24 && h == 24 && model == "general";
        out = nearest4(in, w, h);
        return true;
    };
    ai::Chain c;
    c.tiles = true;
    std::vector<uint32_t> out;
    int W = 0, H = 0;
    std::string error;
    CHECK(ai::runChain(c, o.data(), 64, 64, atlas_tiles::Grid{4, 4}, fake, out, W, H, error));
    CHECK(calls == 16 && sizes);
    CHECK(W == 256 && H == 256 && out == nearest4(o, 64, 64));
    c.tiles = false;  // the whole at once
    calls = 0;
    const ai::Enlarge whole = [&](const std::string&, int, const std::vector<uint32_t>& in, int w, int h,
                                  std::vector<uint32_t>& out2, std::string&) {
        ++calls;
        out2 = nearest4(in, w, h);
        return true;
    };
    CHECK(ai::runChain(c, o.data(), 64, 64, atlas_tiles::Grid{4, 4}, whole, out, W, H, error) && calls == 1);
    const ai::Enlarge broken = [](const std::string&, int, const std::vector<uint32_t>&, int, int,
                                  std::vector<uint32_t>& out3, std::string& why) {
        out3.clear();
        why = "no network";
        return false;
    };
    CHECK(!ai::runChain(c, o.data(), 64, 64, atlas_tiles::Grid{}, broken, out, W, H, error) && error == "no network");
}

TEST(chain_x2_halves_the_network_picture) {
    const std::vector<uint32_t> o = noise(16, 16);
    const ai::Enlarge fake = [](const std::string&, int, const std::vector<uint32_t>& in, int w, int h,
                                std::vector<uint32_t>& out, std::string&) {
        out = nearest4(in, w, h);
        return true;
    };
    ai::Chain c;
    c.scale = 2;
    std::vector<uint32_t> out;
    int W = 0, H = 0;
    std::string error;
    CHECK(ai::runChain(c, o.data(), 16, 16, atlas_tiles::Grid{}, fake, out, W, H, error));
    CHECK(W == 32 && H == 32 && out.size() == 32u * 32u);
    CHECK_EQ(blue(out[0]), blue(o[0]));
}
