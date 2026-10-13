#include <cstring>

#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(frame_strides) {
    CHECK_EQ(FrameRecord::stride(d3d7::FVF_TL1), 32u);
    CHECK_EQ(FrameRecord::stride(d3d7::FVF_TL2), 40u);
}

TEST(frame_states_are_deduplicated_against_the_last_one) {
    FrameRecord f;
    DrawSetup a;
    DrawSetup b;
    b.blend = true;
    CHECK_EQ(f.addState(a), 0u);
    CHECK_EQ(f.addState(a), 0u);
    CHECK_EQ(f.addState(b), 1u);
    CHECK_EQ(f.addState(a), 2u);  // only the last one is compared: cheap and enough for runs of equal draws
    CHECK_EQ(f.states().size(), size_t(3));
}

TEST(frame_draw_copies_vertices_and_indices) {
    FrameRecord f;
    uint8_t v1[2 * 32];
    uint8_t v2[3 * 40];
    for (size_t i = 0; i < sizeof v1; ++i) v1[i] = uint8_t(i);
    for (size_t i = 0; i < sizeof v2; ++i) v2[i] = uint8_t(200 + i % 50);
    const uint16_t idx[3] = {0, 1, 2};
    DrawCmd d;
    d.state = f.addState(DrawSetup{});
    d.primitive = d3d7::PT_TRIANGLESTRIP;
    d.fvf = d3d7::FVF_TL1;
    f.draw(d, v1, 2, nullptr, 0);
    d.fvf = d3d7::FVF_TL2;
    d.primitive = d3d7::PT_TRIANGLELIST;
    f.draw(d, v2, 3, idx, 3);
    CHECK_EQ(f.draws().size(), size_t(2));
    CHECK_EQ(f.draws()[0].vertexOffset, 0u);
    CHECK_EQ(f.draws()[0].vertexCount, 2u);
    CHECK_EQ(f.draws()[0].indexCount, 0u);
    CHECK_EQ(f.draws()[1].vertexOffset, 64u);
    CHECK_EQ(f.draws()[1].firstIndex, 0u);
    CHECK_EQ(f.draws()[1].indexCount, 3u);
    CHECK_EQ(f.vertexBytes().size(), size_t(64 + 120));
    CHECK(std::memcmp(f.vertexBytes().data(), v1, 64) == 0);
    CHECK(std::memcmp(f.vertexBytes().data() + 64, v2, 120) == 0);
    CHECK_EQ(f.indices().size(), size_t(3));
}

TEST(frame_keeps_command_order) {
    FrameRecord f;
    f.clear(ClearCmd{d3d7::CLEAR_TARGET, 0xFF000000, 1.0f, Rect{0, 0, 4, 4}});
    const uint32_t px[4] = {1, 2, 3, 4};
    f.upload(7, 0, Rect{0, 0, 2, 2}, px);
    DrawCmd d;
    d.state = f.addState(DrawSetup{});
    d.fvf = d3d7::FVF_TL1;
    uint8_t v[32] = {};
    f.draw(d, v, 1, nullptr, 0);
    f.writeTarget(Rect{1, 1, 3, 3}, px);
    f.viewport(ViewportCmd{Rect{0, 0, 640, 480}, 0.0f, 1.0f});
    const auto& c = f.commands();
    CHECK_EQ(c.size(), size_t(5));
    CHECK(c[0].type == Cmd::Clear);
    CHECK(c[1].type == Cmd::Upload);
    CHECK(c[2].type == Cmd::Draw);
    CHECK(c[3].type == Cmd::WriteTarget);
    CHECK(c[4].type == Cmd::Viewport);
    CHECK_EQ(f.uploads()[0].texture, 7u);
    CHECK_EQ(f.pixels().size(), size_t(8));
    CHECK_EQ(f.writes()[0].pixels, 4u);
    CHECK_EQ(f.pixels()[f.writes()[0].pixels + 3], 4u);
}

TEST(frame_reset_empties_everything) {
    FrameRecord f;
    f.clear(ClearCmd{d3d7::CLEAR_TARGET, 0, 1.0f, Rect{0, 0, 1, 1}});
    CHECK(!f.empty());
    f.reset();
    CHECK(f.empty());
    CHECK(f.commands().empty());
    CHECK(f.states().empty());
    CHECK(f.pixels().empty());
}
