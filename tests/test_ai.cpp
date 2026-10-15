#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/ai/cache_index.hpp"
#include "core/ai/kinds.hpp"
#include "core/ai/srvgg.hpp"
#include "core/ai/texture_cache.hpp"
#include "test.hpp"

using namespace uagfx::ai;

namespace {

bool loadModel(Model& m, const char* name = "realesr-general-x4v3") {
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/ai/" + name + ".bin", std::ios::binary);
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    std::string error;
    return parseModel(file, m, error);
}

}  // namespace

TEST(ai_network_matches_the_independent_python_forward_pass) {
    // tools: an independent pure-Python forward pass read straight from the .pth gave these values for this 5x4
    // input (channel 0, rows 0 and 5, every third column).
    Model m;
    CHECK(loadModel(m));
    CHECK_EQ(m.middle, 32);
    CHECK_EQ(m.scale, 4);
    const int W = 5, H = 4;
    std::vector<float> rgb(3 * W * H), out(3 * W * H * 16);
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                rgb[size_t((c * H + y) * W + x)] = float((c * 37 + y * 11 + x * 7) % 23) / 22.0f;
    upscale(m, rgb.data(), W, H, out.data(), Border::Zero);
    const float row0[6] = {0.0f, 0.123672f, 0.394143f, 0.634569f, 0.70155f, 0.564371f};
    for (int i = 0; i < 6; ++i) CHECK(std::fabs(out[size_t(i * 3)] - row0[i]) < 1e-4f);
    for (float v : out) CHECK(v >= 0.0f && v <= 1.0f);
}

TEST(ai_wrapped_border_keeps_a_repeating_texture_repeating) {
    // The ground repeats one texture every 4x4 tiles (comodo.gnd: 98% of the sand tiles continue their neighbour's
    // quarter). With a zero border the network made each repeat's edges up from black, and the beach showed a seam
    // every 4 tiles (0.11.1). Wrapped, the network sees the texture as the ground shows it: a texture moved round
    // by (dx, dy) comes out moved round by 4 * (dx, dy), so the enlarged one repeats without a seam too.
    Model m;
    CHECK(loadModel(m));
    const int W = 8, H = 6, dx = 3, dy = 2, S = 4, OW = W * S, OH = H * S;
    std::vector<float> rgb(3 * W * H), moved(3 * W * H), out(3 * OW * OH), outMoved(3 * OW * OH);
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                rgb[size_t((c * H + y) * W + x)] = float((c * 29 + y * 13 + x * 5 + x * y) % 31) / 30.0f;
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < H; ++y)
            for (int x = 0; x < W; ++x)
                moved[size_t((c * H + (y + dy) % H) * W + (x + dx) % W)] = rgb[size_t((c * H + y) * W + x)];
    upscale(m, rgb.data(), W, H, out.data(), Border::Wrap);
    upscale(m, moved.data(), W, H, outMoved.data(), Border::Wrap);
    float worst = 0.0f;
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < OH; ++y)
            for (int x = 0; x < OW; ++x) {
                const float a = out[size_t((c * OH + y) * OW + x)];
                const float b = outMoved[size_t((c * OH + (y + dy * S) % OH) * OW + (x + dx * S) % OW)];
                worst = std::max(worst, std::fabs(a - b));
            }
    CHECK(worst < 1e-5f);
    // The zero border, as Real-ESRGAN was trained, is what the reference check above runs: not this.
    upscale(m, rgb.data(), W, H, out.data(), Border::Zero);
    upscale(m, moved.data(), W, H, outMoved.data(), Border::Zero);
    worst = 0.0f;
    for (int c = 0; c < 3; ++c)
        for (int y = 0; y < OH; ++y)
            for (int x = 0; x < OW; ++x) {
                const float a = out[size_t((c * OH + y) * OW + x)];
                const float b = outMoved[size_t((c * OH + (y + dy * S) % OH) * OW + (x + dx * S) % OW)];
                worst = std::max(worst, std::fabs(a - b));
            }
    CHECK(worst > 0.01f);
}

TEST(ai_models_mix_by_denoise_strength) {
    // Real-ESRGAN's denoise strength for realesr-general-x4v3: its weights and the weak-denoise model's
    // (realesr-general-wdn-x4v3) mixed, d * x4v3 + (1 - d) * wdn. 100 is the model alone (0.11.0-0.11.2, sand came out
    // flat), 50 Real-ESRGAN's default, 0 keeps the most grain.
    Model strong, weak, mixed;
    CHECK(loadModel(strong));
    CHECK(loadModel(weak, "realesr-general-wdn-x4v3"));
    std::string error;
    CHECK(mixModels(strong, weak, 100, mixed, error));
    CHECK(mixed.data == strong.data);
    CHECK(mixModels(strong, weak, 0, mixed, error));
    CHECK(mixed.data == weak.data);
    CHECK(mixModels(strong, weak, 50, mixed, error));
    CHECK_EQ(mixed.middle, strong.middle);
    CHECK_EQ(mixed.convs.size(), strong.convs.size());
    float worst = 0.0f;
    for (size_t i = 0; i < mixed.data.size(); ++i)
        worst = std::max(worst, std::fabs(mixed.data[i] - 0.5f * (strong.data[i] + weak.data[i])));
    CHECK(worst < 1e-6f);
    Model other = weak;
    other.data.pop_back();
    CHECK(!mixModels(strong, other, 50, mixed, error) && !error.empty());  // not the same network
}

TEST(ai_model_file_must_be_whole) {
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/ai/realesr-general-x4v3.bin", std::ios::binary);
    std::vector<uint8_t> file((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    Model m;
    std::string error;
    CHECK(parseModel(file, m, error));
    file.pop_back();
    CHECK(!parseModel(file, m, error));
    CHECK(!parseModel(std::vector<uint8_t>{'N', 'O', 'P', 'E'}, m, error));
}

TEST(ai_texture_key_changes_with_every_texel) {
    std::vector<uint32_t> a(64 * 32);
    for (size_t i = 0; i < a.size(); ++i) a[i] = 0xFF000000u | uint32_t(i * 2654435761u >> 8);
    const TextureKey k = textureKey(a.data(), 64, 32);
    CHECK(k.width == 64 && k.height == 32);
    a[100] ^= 1;
    CHECK(textureKey(a.data(), 64, 32).hash != k.hash);  // one texel differs: another texture
    CHECK(textureKey(a.data(), 32, 64).hash != textureKey(a.data(), 64, 32).hash);
    CHECK(keyName(k) == "64x32\\" + keyName(k).substr(6));  // the size, a backslash and 16 hex digits
    CHECK_EQ(keyName(k).size(), size_t(6 + 16));
}

TEST(ai_keyed_texels_take_their_neighbours_colour_and_alpha_grows_smooth) {
    // A 4x4 texture: magenta keyed away (alpha 0) in its left column, the rest a flat green.
    std::vector<uint32_t> t(16, 0xFF20C040u);
    for (int y = 0; y < 4; ++y) t[size_t(y * 4)] = 0x00FF00FFu;
    CHECK(hasTransparency(t.data(), 4, 4));
    std::vector<float> planes;
    prepareInput(t.data(), 4, 4, planes);
    for (int y = 0; y < 4; ++y) {
        const size_t i = size_t(y * 4);
        CHECK(std::fabs(planes[i] - 0x20 / 255.0f) < 1e-6f);        // red as the green neighbour, not magenta
        CHECK(std::fabs(planes[16 + i] - 0xC0 / 255.0f) < 1e-6f);
        CHECK(std::fabs(planes[32 + i] - 0x40 / 255.0f) < 1e-6f);
        CHECK_EQ(planes[48 + i], 0.0f);                             // still transparent
        CHECK_EQ(planes[48 + i + 1], 1.0f);
    }
    std::vector<uint8_t> alpha;
    enlargeAlpha(planes.data() + 48, 4, 4, 4, alpha);
    CHECK_EQ(alpha.size(), size_t(256));
    CHECK_EQ(alpha[0], 0);        // deep inside the keyed column
    CHECK_EQ(alpha[15], 255);     // well inside the visible part
    // The edge sits half way between the first two texel centres: 4 output pixels in.
    CHECK_EQ(alpha[3], 0);
    CHECK_EQ(alpha[4], 255);
    std::vector<uint32_t> opaque(16, 0xFF102030u);
    CHECK(!hasTransparency(opaque.data(), 4, 4));
}

TEST(ai_draw_kinds) {
    // The ground has two sets of texture coordinates (its lightmap in stage 1); additive blending is light; one quad
    // facing the screen is a sprite; the rest of the world is objects.
    DrawFacts f;
    CHECK(drawKind(f) == Kind::Objects);
    f.flatQuad = true;
    CHECK(drawKind(f) == Kind::Sprites);
    f.additive = true;
    CHECK(drawKind(f) == Kind::Effects);
    f = DrawFacts();
    f.uvSets = 2;
    CHECK(drawKind(f) == Kind::Ground);
    f.stage = 1;
    CHECK(drawKind(f) == Kind::Lightmap);  // never enlarged: its tiles would bleed into each other
    f.uvSets = 1;
    CHECK(drawKind(f) == Kind::None);
    Kind k;
    CHECK(parseKind("sprites", k) && k == Kind::Sprites);
    CHECK(!parseKind("lightmap", k));
    CHECK(std::string(kindName(Kind::Lightmap)).empty());
}

namespace {

std::vector<uint8_t> quad(const float xyzw[4][4], size_t stride) {
    std::vector<uint8_t> v(stride * 4, 0);
    for (int i = 0; i < 4; ++i) std::memcpy(v.data() + i * stride, xyzw[i], 16);
    return v;
}

}  // namespace

TEST(ai_flat_quads_are_sprites) {
    const size_t stride = 32;  // XYZRHW, diffuse, specular, one UV set
    // A sprite: four corners at one depth (a strip).
    const float sprite[4][4] = {{10, 20, 0.5f, 2}, {74, 20, 0.5f, 2}, {10, 148, 0.5f, 2}, {74, 148, 0.5f, 2}};
    CHECK(flatQuad(quad(sprite, stride).data(), stride, 4, 0));
    CHECK(flatQuad(quad(sprite, stride).data(), stride, 4, 6));   // a list through 6 indices
    CHECK(!flatQuad(quad(sprite, stride).data(), stride, 3, 0));  // a triangle
    CHECK(!flatQuad(quad(sprite, stride).data(), stride, 4, 3));
    // A rectangle along the axes with the depth changing (a sprite the game gave a slope): still one.
    const float leaning[4][4] = {{10, 20, 0.4f, 2}, {74, 20, 0.4f, 2}, {74, 148, 0.6f, 2}, {10, 148, 0.6f, 2}};
    CHECK(flatQuad(quad(leaning, stride).data(), stride, 4, 0));
    // A ground tile in perspective: its depth changes and its corners make no rectangle.
    const float tile[4][4] = {{100, 300, 0.6f, 1.5f}, {160, 300, 0.6f, 1.5f}, {90, 340, 0.5f, 1.7f},
                              {170, 340, 0.5f, 1.7f}};
    CHECK(!flatQuad(quad(tile, stride).data(), stride, 4, 0));
    // A rotated quad at one depth (an effect turning): one depth is enough.
    const float turned[4][4] = {{50, 10, 0.3f, 3}, {90, 50, 0.3f, 3}, {10, 50, 0.3f, 3}, {50, 90, 0.3f, 3}};
    CHECK(flatQuad(quad(turned, stride).data(), stride, 4, 0));
    CHECK(!flatQuad(nullptr, stride, 4, 0));
}

TEST(ai_key_names) {
    // A game texture's key as the texture pack names its pictures by it (pack::pictureName).
    TextureKey k{0x0123456789abcdefull, 256, 128};
    CHECK(keyName(k) == "256x128\\0123456789abcdef");
}

TEST(ai_map_names_as_the_game_loads_them) {
    CHECK(mapName("prontera.rsw", 12) == "prontera");
    CHECK(mapName("PRT_FILD08.RSW\0junk", 19) == "prt_fild08");
    CHECK(mapName("1@tower.rsw", 11) == "1@tower");
    CHECK(mapName("new_1-1.rsw", 11) == "new_1-1");
    CHECK(mapName("\x01\x02garbage", 9).empty());
    CHECK(mapName("", 0).empty());
    CHECK(mapName("prontera", 4) == "pron");  // only the bytes given
}

namespace {

// 0.48: the texture pack's pictures, their kinds and maps from its index (the builder's have no map).
std::vector<CacheEntry> sampleCache() {
    return {
        {"world/256x256/0000000000000001.webp", 100, Kind::Ground, {"prontera", "geffen"}},
        {"world/256x256/0000000000000002.webp", 200, Kind::Objects, {"prontera"}},
        {"sprites/128x128/0000000000000003.webp", 30, Kind::Sprites, {"geffen"}},
        {"world/64x64/0000000000000004.webp", 10, Kind::None, {}},  // not in the index: no kind, no map
        {"world/64x64/0000000000000005.webp", 5, Kind::Ground, {}},
    };
}

}  // namespace

TEST(ai_cache_index_by_map_and_kind) {
    const CacheContents c(sampleCache());
    CHECK_EQ(c.pictures(), size_t(5));
    const CacheIndex x = c.index();
    CHECK_EQ(x.all.total, 5u);
    CHECK_EQ(x.all.bytes, uint64_t(345));
    CHECK_EQ(x.all.count[0], 2u);
    CHECK_EQ(x.all.count[1], 1u);
    CHECK_EQ(x.all.count[2], 1u);
    CHECK_EQ(x.rows.size(), size_t(3));  // geffen, prontera, then the pictures of no map
    CHECK(x.rows[0].map == "geffen");
    CHECK_EQ(x.rows[0].total, 2u);  // 1 (shared with prontera) and 3
    CHECK_EQ(x.rows[0].bytes, uint64_t(130));
    CHECK_EQ(x.rows[0].count[2], 1u);
    CHECK(x.rows[1].map == "prontera");
    CHECK_EQ(x.rows[1].total, 2u);
    CHECK_EQ(x.rows[1].count[0], 1u);
    CHECK_EQ(x.rows[1].count[1], 1u);
    CHECK(x.rows[2].map.empty());
    CHECK_EQ(x.rows[2].total, 2u);  // 4 (of no kind: in the total only) and 5
    CHECK_EQ(x.rows[2].count[0], 1u);
    CHECK_EQ(x.rows[2].count[1] + x.rows[2].count[2] + x.rows[2].count[3] + x.rows[2].count[4], 0u);
}

TEST(ai_cache_deletion_by_map_and_kind) {
    const CacheContents c(sampleCache());
    const auto has = [](const std::vector<std::string>& names, const std::string& n) {
        return std::find(names.begin(), names.end(), n) != names.end();
    };
    const std::string p1 = "world/256x256/0000000000000001.webp", p2 = "world/256x256/0000000000000002.webp";
    // Prontera's ground only: 1 goes (from geffen too - it is the same texture), 2 (objects) stays.
    CacheDeletion d;
    d.maps = {"prontera"};
    d.kinds[1] = false;
    std::vector<std::string> keep = c.keep(d);
    CHECK(!has(keep, p1));
    CHECK(has(keep, p2));
    CHECK_EQ(keep.size(), size_t(4));
    // Every kind of prontera.
    d.kinds[1] = true;
    keep = c.keep(d);
    CHECK(!has(keep, p1) && !has(keep, p2));
    CHECK_EQ(keep.size(), size_t(3));
    // The row of no map: a picture of no kind goes whatever kinds are chosen, the others by their kind.
    CacheDeletion none;
    none.maps = {""};
    for (bool& k : none.kinds) k = false;
    keep = c.keep(none);
    CHECK(!has(keep, "world/64x64/0000000000000004.webp"));
    CHECK(has(keep, "world/64x64/0000000000000005.webp"));
    none.kinds[0] = true;
    keep = c.keep(none);
    CHECK(!has(keep, "world/64x64/0000000000000005.webp"));
    CHECK_EQ(keep.size(), size_t(3));
}

TEST(ai_anime_model_is_the_compact_one) {
    // realesr-animevideov3 (the Drawn model): the same network with 16 convolutions in the middle instead of 32 - the
    // GPU's shaders read the count from the file. A flat picture stays flat and keeps its colour.
    Model m;
    CHECK(loadModel(m, "realesr-animevideov3"));
    CHECK_EQ(m.middle, 16);
    CHECK_EQ(m.features, 64);
    CHECK_EQ(m.scale, 4);
    CHECK_EQ(m.convs.size(), size_t(18));
    const int w = 8, h = 8;
    std::vector<float> rgb(size_t(3 * w * h));
    for (int c = 0; c < 3; ++c)
        for (int i = 0; i < w * h; ++i) rgb[size_t(c * w * h + i)] = c == 0 ? 0.8f : c == 1 ? 0.5f : 0.2f;
    std::vector<float> out(size_t(3 * w * h * 16));
    upscale(m, rgb.data(), w, h, out.data(), Border::Wrap);
    float worst = 0;
    for (int c = 0; c < 3; ++c)
        for (int i = 0; i < w * h * 16; ++i)
            worst = std::max(worst, std::fabs(out[size_t(c * w * h * 16 + i)] - rgb[size_t(c * w * h)]));
    CHECK(worst < 0.05f);
}

TEST(ai_alpha_through_the_network_keeps_thin_lines_whole) {
    // 0.13.1: a sprite's thin diagonal line (texels touching at their corners). The bilinear alpha cut at one half
    // breaks it into dots at every corner; the network's own alpha (Real-ESRGAN's way for RGBA pictures) keeps it one
    // line - the beads the first sprites of 0.13.0 showed at their thin edges.
    Model m;
    CHECK(loadModel(m, "realesr-animevideov3"));
    const int w = 16, h = 16, s = 4;
    std::vector<float> alpha(size_t(w * h), 0.0f);
    for (int i = 3; i < 13; ++i) alpha[size_t(i * w + i)] = 1.0f;
    const auto connected = [&](const std::vector<uint8_t>& a) {
        // Along the line's middle, every enlarged pixel between its first and last texel's centre is opaque.
        for (int t = 3 * s + s / 2; t <= 12 * s + s / 2; ++t)
            if (!a[size_t(t * w * s + t)]) return false;
        return true;
    };
    std::vector<uint8_t> bilinear;
    enlargeAlpha(alpha.data(), w, h, s, bilinear);
    std::vector<uint8_t> network;
    upscaleAlpha(m, alpha.data(), w, h, network, Border::Zero);
    CHECK_EQ(network.size(), size_t(w * h * s * s));
    CHECK(connected(network));
    // Away from the line it stays transparent: no halo.
    CHECK(!network[size_t((2 * s) * w * s + 13 * s)]);
    CHECK(!network[size_t((13 * s) * w * s + 2 * s)]);
    // The diagonal through the corners: the network fills it, the bilinear cut leaves it at exactly one half.
    size_t gaps = 0;
    for (int t = 3 * s + s / 2; t <= 12 * s + s / 2; ++t) gaps += bilinear[size_t(t * w * s + t + 1)] ? 0 : 1;
    CHECK(gaps > 0);
}

TEST(ai_clamped_edges_do_not_darken_an_opaque_picture) {
    // 0.15, an interface picture (a button, an item's picture) is opaque to its edge: with zeros past it the network
    // darkens the edge; with the edge pixel repeated a flat picture stays flat.
    Model m;
    CHECK(loadModel(m, "realesr-animevideov3"));
    const int w = 8, h = 8;
    std::vector<float> rgb(size_t(3 * w * h));
    for (int c = 0; c < 3; ++c)
        for (int i = 0; i < w * h; ++i) rgb[size_t(c * w * h + i)] = c == 0 ? 0.9f : c == 1 ? 0.8f : 0.7f;
    const size_t big = size_t(w * h * 16);
    std::vector<float> clamped(3 * big), zero(3 * big);
    upscale(m, rgb.data(), w, h, clamped.data(), Border::Clamp);
    upscale(m, rgb.data(), w, h, zero.data(), Border::Zero);
    const auto corner = [&](const std::vector<float>& out) { return out[0] + out[big] + out[2 * big]; };
    const auto middle = [&](const std::vector<float>& out) {
        const size_t i = size_t((h * 2) * (w * 4) + w * 2);
        return out[i] + out[big + i] + out[2 * big + i];
    };
    CHECK(std::fabs(corner(clamped) - middle(clamped)) < 0.03f);
    CHECK(std::fabs(corner(zero) - middle(zero)) > std::fabs(corner(clamped) - middle(clamped)));
}

TEST(ai_soft_pictures_are_glows_and_gradients) {
    // 0.22.2: light alone - a glow, a ramp, one colour - is soft; grain or a hard edge is not.
    const int n = 64;
    std::vector<uint32_t> glow(size_t(n) * n), ramp(size_t(n) * n), flat(size_t(n) * n, 0xFF336699u);
    std::vector<uint32_t> grain(size_t(n) * n), disc(size_t(n) * n);
    uint32_t seed = 12345;
    for (int y = 0; y < n; ++y)
        for (int x = 0; x < n; ++x) {
            const float dx = float(x) - 31.5f, dy = float(y) - 31.5f;
            const auto g = uint32_t(255.0f * std::exp(-(dx * dx + dy * dy) / (2.0f * 12.0f * 12.0f)));
            glow[size_t(y) * n + x] = 0xFF000000u | g << 16 | (g * 3 / 4) << 8 | g / 4;
            const uint32_t r = uint32_t(x * 4);
            ramp[size_t(y) * n + x] = 0xFF000000u | r << 16 | r << 8 | r;
            seed = seed * 1103515245u + 12345u;
            const uint32_t v = 96 + (seed >> 16) % 64;  // a texture's grain: +-32
            grain[size_t(y) * n + x] = 0xFF000000u | v << 16 | v << 8 | v;
            disc[size_t(y) * n + x] = dx * dx + dy * dy < 20.0f * 20.0f ? 0xFFC08040u : 0u;
        }
    CHECK(softPicture(glow.data(), n, n));
    CHECK(softPicture(ramp.data(), n, n));
    CHECK(softPicture(flat.data(), n, n));
    CHECK(!softPicture(grain.data(), n, n));
    CHECK(!softPicture(disc.data(), n, n));
    CHECK(!softPicture(glow.data(), 2, 2));  // too small to tell
}

TEST(ai_soft_alpha_pictures_stay_the_games) {
    // 0.39.2: a picture whose shape is in a soft alpha - the chimneys' smoke, a skill's glowing particle (Soul Strike):
    // its colour flat, more than 5 % of its texels part-transparent. The network's alpha is all or nothing (made for
    // the colour key): smoke turned into a hard white blob, a soft particle into a small square.
    std::vector<uint32_t> glow(64 * 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const float d = std::hypot(float(x) - 31.5f, float(y) - 31.5f) / 32.0f;
            const uint32_t a = uint32_t(255.0f * std::max(0.0f, 1.0f - d));
            glow[size_t(y) * 64 + size_t(x)] = a << 24 | 0xFFFFFFu;
        }
    CHECK(softAlpha(glow.data(), 64, 64));
    std::vector<uint32_t> keyed(64 * 64, 0xFF808080u);  // a sprite: its texels all or nothing
    for (int i = 0; i < 64 * 20; ++i) keyed[size_t(i)] = 0x00FF00FFu;
    CHECK(!softAlpha(keyed.data(), 64, 64));
    std::vector<uint32_t> edge(64 * 64, 0xFF808080u);  // a few soft texels along an edge: not enough
    for (int i = 0; i < 64; ++i) edge[size_t(i)] = 0x80808080u;
    CHECK(!softAlpha(edge.data(), 64, 64));
    CHECK(!softAlpha(nullptr, 0, 0));
}

TEST(ai_halves_a_pack_picture_without_dark_edges) {
    // 0.42: the texture pack's pictures are 4 times the game's - at [ai] scale = 2 half of that. A red texel beside
    // transparent ones stays red (only less opaque): nothing transparent darkens the colour.
    std::vector<uint32_t> argb = {0xFFFF0000u, 0x00000000u, 0xFF0000FFu, 0xFF0000FFu,
                                  0x00000000u, 0x00000000u, 0xFF0000FFu, 0xFF0000FFu};
    int w = 4, h = 2;
    halve(argb, w, h);
    CHECK(w == 2 && h == 1 && argb.size() == 2);
    CHECK_EQ(argb[0] >> 24, 64u);                // a quarter opaque
    CHECK_EQ(argb[0] & 0xFFFFFFu, 0xFF0000u);    // and still red
    CHECK_EQ(argb[1], 0xFF0000FFu);
}
