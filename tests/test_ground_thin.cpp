// 3.0.19-beta.42 (item 5 of the owner's «делаем все по порядку»: the speed): in the 3D view the game works out each of
// its ground's cells (some 8000 a frame to the sight) and the driver records each, while the renderer's own ground is
// drawn in their place - the game's only teaching it their colours and fog. One past the reflections' casters' reach
// only one frame in kEvery (its turn by its address), the near ones every frame
#include <set>

#include "core/config.hpp"
#include "core/ground_thin.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(ground_thin_near_cells_every_frame) {
    for (uint32_t frame = 0; frame < 20; ++frame) {
        CHECK(ground_thin::workedOut(10.0f, 0x01230040u, frame));
        CHECK(ground_thin::workedOut(ground_thin::kNearDepth, 0x01230040u, frame));
        CHECK(ground_thin::workedOut(-5.0f, 0x01230040u, frame));  // behind the eye: the game's own culling
    }
}

TEST(ground_thin_far_cells_once_in_their_turn) {
    const float far = ground_thin::kNearDepth + 100.0f;
    for (uint32_t key = 0x02000000u; key < 0x02000000u + 64 * 0x80u; key += 0x80u) {
        int times = 0;
        for (uint32_t frame = 0; frame < ground_thin::kEvery; ++frame) times += ground_thin::workedOut(far, key, frame);
        CHECK(times == 1);  // once in every kEvery frames, each
    }
    // spread over the frames: of 800 cells (0x80 bytes apart, as the game's blocks) some a frame, not all at once
    std::set<uint32_t> frames;
    for (uint32_t frame = 0; frame < ground_thin::kEvery; ++frame) {
        int n = 0;
        for (uint32_t k = 0; k < 800; ++k) n += ground_thin::workedOut(far, 0x03000000u + k * 0x80u, frame);
        CHECK(n > 40 && n < 160);
        if (n) frames.insert(frame);
    }
    CHECK(frames.size() == ground_thin::kEvery);
}

// [view3d] fast_ground - on unless the file says 0
TEST(config_view3d_fast_ground_on_by_default) {
    CHECK(parseConfig("").view3dFastGround);
    CHECK(!parseConfig("[view3d]\nfast_ground = 0\n").view3dFastGround);
    const settings::IniSetting* row = settings::findSetting("view3d.fast_ground");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && settings::tokenOf(*row, parseConfig("")) == "1");
    CHECK(appliesNow("view3d.fast_ground"));
    CHECK(!liveUpdate(parseConfig(""), parseConfig("[view3d]\nfast_ground = 0\n")).next.view3dFastGround);
}
