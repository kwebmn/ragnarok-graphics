// 3.0.18-beta.181 (the owner: Storm Gust «как в игре, только чётко»): a texture of small separate bright dots on black
// (Storm Gust's snow) - its dots found and drawn again at real resolution, each one's place, spread, colour and light
// as the texture had them
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "core/sharp_specks.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

uint32_t argb(float a, float r, float g, float b) {
    const auto c = [](float v) { return uint32_t(std::lround(std::fmin(std::fmax(v, 0.0f), 1.0f) * 255)); };
    return c(a) << 24 | c(r) << 16 | c(g) << 8 | c(b);
}

// A sheet w x h: black (alpha 0) with round dots of radius `r` texels at the given places
std::vector<uint32_t> sheet(int w, int h, const std::vector<std::pair<float, float>>& at, float r, float bright) {
    std::vector<uint32_t> px(size_t(w) * size_t(h), 0);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            for (const auto& [cx, cy] : at) {
                const float d = std::hypot(float(x) + 0.5f - cx, float(y) + 0.5f - cy);
                if (d < r) px[size_t(y) * size_t(w) + size_t(x)] = argb(1, bright, bright, bright * 0.95f);
            }
    return px;
}

std::vector<std::pair<float, float>> grid(int w, int h, int n) {
    std::vector<std::pair<float, float>> at;
    const int across = std::max(1, (w - 8) / 9);  // apart: 9 texels each way
    for (int i = 0; i < n; ++i) at.emplace_back(6.0f + 9.0f * float(i % across), 6.0f + 9.0f * float(i / across));
    return at;
}

float light(const std::vector<uint32_t>& px) {  // all the light an added layer gives: colour times alpha
    double sum = 0;
    for (uint32_t p : px) sum += double((p >> 16) & 0xFF) / 255.0 * double(p >> 24) / 255.0;
    return float(sum);
}

}  // namespace

TEST(sharp_specks_finds_a_sheet_of_snow) {
    const std::vector<uint32_t> snow = sheet(64, 64, grid(64, 64, 14), 1.6f, 0.95f);
    const std::vector<sharpspecks::Speck> found = sharpspecks::specksOf(snow.data(), 64, 64);
    CHECK(found.size() == 14);
    bool near = false;  // the first dot at 6.0, 6.0
    for (const sharpspecks::Speck& s : found)
        near = near || (std::fabs(s.x - 6.0f) < 0.3f && std::fabs(s.y - 6.0f) < 0.3f);
    CHECK(near);
    CHECK(found[0].peak > 0.9f && found[0].energy > 4 && found[0].b < found[0].r);
}

TEST(sharp_specks_not_a_cloud_nor_a_few_dots) {
    std::vector<uint32_t> cloud(64 * 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            const float d = std::hypot(float(x) - 32.0f, float(y) - 32.0f) / 30.0f;
            cloud[size_t(y) * 64 + size_t(x)] = argb(1, 1 - d, 1 - d, 1 - d);
        }
    CHECK(sharpspecks::specksOf(cloud.data(), 64, 64).empty());  // one big glow
    const std::vector<uint32_t> few = sheet(64, 64, grid(64, 64, 3), 1.6f, 0.95f);
    CHECK(sharpspecks::specksOf(few.data(), 64, 64).empty());  // three dots are no snow
    CHECK(sharpspecks::specksOf(nullptr, 64, 64).empty());
}

TEST(sharp_specks_two_touching_dots_two_specks) {
    std::vector<std::pair<float, float>> at = grid(64, 64, 10);
    at.emplace_back(40.0f, 50.0f);
    at.emplace_back(43.2f, 50.0f);  // two dots that touch: their brightest texels apart
    std::vector<uint32_t> snow = sheet(64, 64, at, 1.7f, 0.9f);
    for (int y = 47; y <= 52; ++y)  // a dimmer neck between them, where they meet
        for (int x = 41; x <= 42; ++x) {
            uint32_t& p = snow[size_t(y) * 64 + size_t(x)];
            if (p >> 24) p = argb(1, 0.45f, 0.45f, 0.45f);
        }
    CHECK(sharpspecks::specksOf(snow.data(), 64, 64).size() == 12);
}

TEST(sharp_specks_redrawn_with_as_much_light_and_a_sharp_edge) {
    const std::vector<uint32_t> snow = sheet(64, 64, grid(64, 64, 14), 1.6f, 0.95f);
    const std::vector<sharpspecks::Speck> found = sharpspecks::specksOf(snow.data(), 64, 64);
    const std::vector<uint32_t> big = sharpspecks::redraw(found, 64, 64, 8);
    CHECK(big.size() == size_t(512) * 512);
    const float game = light(snow), ours = light(big) / 64.0f;  // per texel of the game's
    CHECK(ours > game * 0.8f && ours < game * 1.2f);
    // across the first dot's middle row: dark, then its edge - half its middle's light within two pixels of the copy
    const int row = int(6.0f * 8);
    const uint32_t middle = big[size_t(row) * 512 + size_t(6 * 8)] >> 24;
    int first = -1, half = -1;
    for (int x = 0; x < 120; ++x) {
        const uint32_t a = big[size_t(row) * 512 + size_t(x)] >> 24;
        if (first < 0 && a > 8) first = x;
        if (half < 0 && a * 2 > middle) half = x;
    }
    CHECK(middle > 200 && first > 20 && half >= first && half - first <= 2);
}

TEST(sharp_specks_copy_and_its_levels) {
    const std::vector<uint32_t> snow = sheet(64, 64, grid(64, 64, 14), 1.6f, 0.95f);
    const sharpspecks::Copy c = sharpspecks::makeCopy(snow.data(), 64, 64);
    CHECK(c.width == 512 && c.height == 512 && c.specks == 14);
    CHECK(c.levels.size() == 4);  // 512, 256, 128, 64
    CHECK(c.levels.size() == 4 && c.levels[3].size() == size_t(64) * 64);
    const std::vector<uint32_t> wide = sheet(256, 128, grid(256, 128, 20), 2.0f, 0.9f);
    const sharpspecks::Copy w = sharpspecks::makeCopy(wide.data(), 256, 128);
    CHECK(w.width == 512 && w.height == 256 && w.levels.size() == 2);
    CHECK(sharpspecks::makeCopy(nullptr, 64, 64).levels.empty());
}
