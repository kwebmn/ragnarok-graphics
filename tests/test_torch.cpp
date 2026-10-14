#include <cmath>

#include "core/sprite_exe.hpp"
#include "core/torch.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
bool near(float a, float b, float eps = 1e-3f) { return std::fabs(a - b) <= eps; }
}  // namespace

TEST(sprite_exe_knows_the_torch_file) {
    // 0.23: the map torches' sprite, effect\torch_01, its frames told apart from the other effects'.
    using sprite_exe::isTorch;
    CHECK(isTorch("data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\torch_01.spr"));  // 이팩트
    CHECK(isTorch("\xC0\xCC\xC6\xD1\xC6\xAE\\TORCH_01.SPR"));                // relative, any case
    CHECK(!isTorch("data\\sprite\\\xC0\xCC\xC6\xD1\xC6\xAE\\torch_02.spr"));
    CHECK(!isTorch("data\\sprite\\\xB8\xF3\xBD\xBA\xC5\xCD\\torch_01.spr"));  // 몬스터: not the effect
    CHECK(!isTorch(nullptr));
}

TEST(torch_cells_of_whole_textures) {
    // 0.23.1: the game gives each 32-bit frame a texture of its own at load (CSprRes::Load) and draws it from there,
    // not from its sprite cache: the texture is the layer as a whole, its frame in the top left corner.
    torch::Cells cells;
    cells.noteWhole(9, torch::Layer::Flame);
    Rect where;
    CHECK(cells.at(9, 100.0f, 120.0f, &where) == torch::Layer::Flame);
    CHECK(where.left == 0 && where.top == 0);
    cells.noteWhole(10, torch::Layer::Glow);
    CHECK(cells.at(10, 3.0f, 3.0f) == torch::Layer::Glow);
    cells.noteWhole(9, torch::Layer::None);  // another picture now
    CHECK(cells.at(9, 100.0f, 120.0f) == torch::Layer::None);
    cells.forget(10);
    CHECK(cells.empty());
}

TEST(torch_flame_of_an_upright_quad) {
    // A flame frame in the right half of a 256 x 256 texture, drawn twice its size with its top left at (100, 50):
    // its base is texel (65, 103) of the frame, its tip 58 texels higher, its half width 14 texels.
    const torch::Corner q[4] = {{100, 50, 0.5f, 0.01f, 0.5f, 0.0f},
                                {356, 50, 0.5f, 0.01f, 1.0f, 0.0f},
                                {100, 306, 0.5f, 0.01f, 0.5f, 0.5f},
                                {356, 306, 0.5f, 0.01f, 1.0f, 0.5f}};
    torch::Flame f;
    CHECK(torch::flameOf(q, f));
    CHECK(near(f.baseX, 230.0f));
    CHECK(near(f.baseY, 256.0f));
    CHECK(near(f.upX, 0.0f));
    CHECK(near(f.upY, -116.0f));
    CHECK(near(f.halfWidth, 28.0f));
    CHECK(near(f.z, 0.5f));
    CHECK(near(f.rhw, 0.01f));
}

TEST(torch_flame_of_a_turned_quad) {
    // The same frame turned a quarter (the frame's up pointing to the screen's right) at its own size.
    const torch::Corner q[4] = {{228, 0, 0.3f, 0.02f, 0.0f, 0.0f},   // texel (0, 0)
                                {228, 128, 0.3f, 0.02f, 0.5f, 0.0f},  // texel (128, 0)
                                {100, 0, 0.3f, 0.02f, 0.0f, 0.5f},    // texel (0, 128)
                                {100, 128, 0.3f, 0.02f, 0.5f, 0.5f}};
    torch::Flame f;
    CHECK(torch::flameOf(q, f));
    CHECK(near(f.baseX, 228.0f - 103.0f));
    CHECK(near(f.baseY, 65.0f));
    CHECK(near(f.upX, 58.0f));
    CHECK(near(f.upY, 0.0f));
    CHECK(near(f.halfWidth, 14.0f));
}

TEST(torch_flame_needs_a_quad_that_maps_the_texture) {
    const torch::Corner flat[4] = {{0, 0, 0.5f, 0.1f, 0.2f, 0.2f}, {10, 0, 0.5f, 0.1f, 0.2f, 0.2f},
                                   {0, 10, 0.5f, 0.1f, 0.2f, 0.2f}, {10, 10, 0.5f, 0.1f, 0.2f, 0.2f}};
    torch::Flame f;
    CHECK(!torch::flameOf(flat, f));
    const torch::Corner line[4] = {{0, 0, 0.5f, 0.1f, 0.0f, 0.0f}, {10, 0, 0.5f, 0.1f, 0.5f, 0.0f},
                                   {20, 0, 0.5f, 0.1f, 1.0f, 0.0f}, {30, 0, 0.5f, 0.1f, 0.0f, 0.0f}};
    CHECK(!torch::flameOf(line, f));  // no height in the texture
}

TEST(torch_flame_of_a_halved_frame) {
    // The game's sprite setting "half": the frame is 64 x 64 texels in a 64 x 64 texture, drawn at the same size on
    // the screen - the flame lands where it does at full size.
    const torch::Corner q[4] = {{100, 50, 0.5f, 0.01f, 0.0f, 0.0f},
                                {356, 50, 0.5f, 0.01f, 1.0f, 0.0f},
                                {100, 306, 0.5f, 0.01f, 0.0f, 1.0f},
                                {356, 306, 0.5f, 0.01f, 1.0f, 1.0f}};
    torch::Flame f;
    CHECK(torch::flameOf(q, f));
    CHECK(near(f.baseX, 230.0f));
    CHECK(near(f.baseY, 256.0f));
    CHECK(near(f.upY, -116.0f));
    CHECK(near(f.halfWidth, 28.0f));
}

TEST(torch_flicker_stays_within_its_amount) {
    float lo = 10.0f, hi = -10.0f;
    bool differs = false;
    for (int i = 0; i < 2000; ++i) {
        const double t = i * 0.01;
        const float f = torch::flicker(t, 7, 0.3f);
        lo = std::fmin(lo, f);
        hi = std::fmax(hi, f);
        differs = differs || std::fabs(f - torch::flicker(t, 8, 0.3f)) > 0.01f;
        CHECK(torch::flicker(t, 7, 0.0f) == 1.0f);
    }
    CHECK(lo >= 0.7f - 1e-5f);
    CHECK(hi <= 1.3f + 1e-5f);
    CHECK(hi - lo > 0.25f);  // it does flicker
    CHECK(differs);          // each torch its own way
    // Smooth: a hundredth of a second moves it a little.
    CHECK(std::fabs(torch::flicker(3.0, 7, 0.3f) - torch::flicker(3.01, 7, 0.3f)) < 0.12f);
}

TEST(torch_seed_holds_still_at_its_place) {
    CHECK(torch::seedAt(12.3f, 45.6f) == torch::seedAt(12.4f, 45.5f));  // the camera moved: the same torch
    CHECK(torch::seedAt(12.3f, 45.6f) != torch::seedAt(32.0f, 45.6f));
    CHECK(torch::seedAt(12.3f, 45.6f) != torch::seedAt(12.3f, 65.6f));
}
