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
    CHECK(sprite_exe::isEffect(sprite_exe::Source::Effect));
    CHECK(sprite_exe::isEffect(sprite_exe::Source::TorchGlow));
    CHECK(sprite_exe::isEffect(sprite_exe::Source::TorchFlame));
    CHECK(!sprite_exe::isEffect(sprite_exe::Source::Monster));
}

TEST(torch_cells_follow_the_sprite_cache) {
    // A cell holds a torch frame from its upload until another frame is written into it.
    torch::Cells cells;
    CHECK(cells.empty());
    const SpriteCell first[2] = {{Rect{0, 0, 128, 128}, uint8_t(sprite_exe::Source::TorchFlame)},
                                 {Rect{128, 0, 256, 128}, uint8_t(sprite_exe::Source::Monster)}};
    cells.note(5, first, 2);
    CHECK(!cells.empty());
    Rect where;
    CHECK(cells.at(5, 60.0f, 60.0f, &where) == torch::Layer::Flame);
    CHECK(where == (Rect{0, 0, 128, 128}));
    CHECK(cells.at(5, 200.0f, 60.0f) == torch::Layer::None);
    CHECK(cells.at(6, 60.0f, 60.0f) == torch::Layer::None);  // another texture
    const SpriteCell glow = {Rect{192, 192, 256, 256}, uint8_t(sprite_exe::Source::TorchGlow)};
    cells.note(5, &glow, 1);
    CHECK(cells.at(5, 220.0f, 230.0f) == torch::Layer::Glow);
    const SpriteCell monster = {Rect{0, 0, 128, 128}, uint8_t(sprite_exe::Source::Monster)};
    cells.note(5, &monster, 1);  // the flame's cell taken by a monster's frame
    CHECK(cells.at(5, 60.0f, 60.0f) == torch::Layer::None);
    CHECK(cells.at(5, 220.0f, 230.0f) == torch::Layer::Glow);
    cells.forget(5);
    CHECK(cells.empty());
}

TEST(torch_flame_of_an_upright_quad) {
    // A flame frame in the cell (128, 0) of a 256 x 256 cache, drawn twice its size with its top left at (100, 50):
    // its base is texel (65, 103) of the frame, its tip 58 texels higher, its half width 14 texels.
    const torch::Corner q[4] = {{100, 50, 0.5f, 0.01f, 0.5f, 0.0f},
                                {356, 50, 0.5f, 0.01f, 1.0f, 0.0f},
                                {100, 306, 0.5f, 0.01f, 0.5f, 0.5f},
                                {356, 306, 0.5f, 0.01f, 1.0f, 0.5f}};
    torch::Flame f;
    CHECK(torch::flameOf(q, 256, 256, Rect{128, 0, 256, 128}, f));
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
    CHECK(torch::flameOf(q, 256, 256, Rect{0, 0, 128, 128}, f));
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
    CHECK(!torch::flameOf(flat, 256, 256, Rect{0, 0, 128, 128}, f));
    const torch::Corner q[4] = {{0, 0, 0.5f, 0.1f, 0.0f, 0.0f}, {10, 0, 0.5f, 0.1f, 0.5f, 0.0f},
                                {0, 10, 0.5f, 0.1f, 0.0f, 0.5f}, {10, 10, 0.5f, 0.1f, 0.5f, 0.5f}};
    CHECK(!torch::flameOf(q, 0, 256, Rect{0, 0, 128, 128}, f));
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
