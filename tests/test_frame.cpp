#include <cstring>
#include <functional>
#include <new>

#include "core/d3d7.hpp"
#include "core/frame.hpp"
#include "fail_alloc.hpp"
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

TEST(frame_is_full_after_16_mb) {
    // A map's first frame uploads every texture it draws; past 16 MB the driver plays the record at once instead of
    // growing the GPU upload buffers in a game that has 2 GB of address space.
    FrameRecord r;
    const std::vector<uint32_t> mb4(1024 * 1024, 0xFF00FF00u);
    for (int i = 0; i < 3; ++i) r.upload(1, 0, Rect{0, 0, 1024, 1024}, mb4.data());
    CHECK_EQ(r.pendingBytes(), size_t(12) << 20);
    CHECK(!r.full());
    r.writeTarget(Rect{0, 0, 1024, 1024}, mb4.data());
    CHECK(r.full());
    r.reset();
    CHECK_EQ(r.pendingBytes(), size_t(0));
    CHECK(!r.full());
}

TEST(frame_counts_vertices_and_indices_too) {
    FrameRecord r;
    DrawCmd d;
    d.fvf = d3d7::FVF_TL2;
    const std::vector<uint8_t> vertices(40 * 4, 0);
    const uint16_t indices[6] = {0, 1, 2, 2, 1, 3};
    r.draw(d, vertices.data(), 4, indices, 6);
    CHECK_EQ(r.pendingBytes(), size_t(40 * 4 + 6 * 2));
}

TEST(frame_draw_carries_a_window_twin) {
    CHECK_EQ(DrawCmd{}.twin, 0u);  // none unless the driver attaches one (stage 6)
    FrameRecord f;
    DrawCmd cmd;
    cmd.fvf = d3d7::FVF_TL1;
    cmd.twin = 7;
    cmd.twinTile[0] = 256;
    cmd.twinTile[1] = 0;
    cmd.twinScale = 1.75;
    const uint8_t vertices[32 * 3] = {};
    f.draw(cmd, vertices, 3, nullptr, 0);
    CHECK_EQ(f.draws()[0].twin, 7u);
    CHECK_EQ(f.draws()[0].twinTile[0], 256);
    CHECK(f.draws()[0].twinScale == 1.75);
}

TEST(frame_record_out_of_memory_leaves_no_command_without_its_data) {
    // Each call adds its data, then its command (0.5.2 review 2a): memory running out between the two must leave no
    // command whose data is missing - execute would read past the end. Three commands of another kind leave the
    // command list room for a fourth, so the call allocates only for its own data.
    const uint32_t pixels[4] = {1, 2, 3, 4};
    const uint8_t vertices[32] = {};
    const auto survives = [](bool clears, int allowed, const std::function<void(FrameRecord&)>& add) {
        FrameRecord r;
        for (int i = 0; i < 3; ++i) {
            if (clears)
                r.clear(ClearCmd{});
            else
                r.viewport(ViewportCmd{});
        }
        bool threw = false;
        {
            t::FailAllocations fail(allowed);
            try {
                add(r);
            } catch (const std::bad_alloc&) {
                threw = true;
            }
        }
        for (const Command& c : r.commands()) {
            const size_t n = c.type == Cmd::Clear      ? r.clears().size()
                             : c.type == Cmd::Viewport ? r.viewports().size()
                             : c.type == Cmd::Draw     ? r.draws().size()
                             : c.type == Cmd::Upload   ? r.uploads().size()
                                                       : r.writes().size();
            if (c.index >= n) return false;
        }
        return threw && r.commands().size() == 3;
    };
    CHECK(survives(false, 0, [](FrameRecord& r) { r.clear(ClearCmd{}); }));
    CHECK(survives(true, 0, [](FrameRecord& r) { r.viewport(ViewportCmd{}); }));
    CHECK(survives(true, 1, [&](FrameRecord& r) { r.upload(7, 0, Rect{0, 0, 2, 2}, pixels); }));  // its pixels go in
    CHECK(survives(true, 1, [&](FrameRecord& r) { r.writeTarget(Rect{0, 0, 2, 2}, pixels); }));
    CHECK(survives(true, 1, [&](FrameRecord& r) {  // its vertices go in
        DrawCmd d;
        d.fvf = d3d7::FVF_TL1;
        r.draw(d, vertices, 1, nullptr, 0);
    }));
}

TEST(frame_sprite_source_is_the_cell_a_draw_samples) {
    // 2.2.2: a draw of the game's sprite cache shows the frame of the cell its texture coordinates fall in - the
    // middle of its vertices' (u, v 24 bytes into a transformed vertex): a quad's corners lie on the cell's edges
    const std::vector<SpriteCell> cells = {{Rect{0, 0, 64, 64}, 1}, {Rect{64, 0, 128, 96}, 3}};
    CHECK_EQ(int(spriteSourceAt(cells, 256, 256, 0.1f, 0.1f)), 1);
    CHECK_EQ(int(spriteSourceAt(cells, 256, 256, 0.3f, 0.2f)), 3);
    CHECK_EQ(int(spriteSourceAt(cells, 256, 256, 0.9f, 0.9f)), 0);  // no cell there: unknown
    std::vector<uint8_t> bytes(8 + 4 * 32, 0);
    const float uv[4][2] = {{0.25f, 0.0f}, {0.5f, 0.0f}, {0.25f, 0.375f}, {0.5f, 0.375f}};
    for (int i = 0; i < 4; ++i) std::memcpy(bytes.data() + 8 + i * 32 + 24, uv[i], sizeof uv[i]);
    CHECK_EQ(int(spriteSourceOf(cells, 256, 256, bytes, 8, 32, 4)), 3);
    CHECK_EQ(int(spriteSourceOf(cells, 256, 256, bytes, 8, 32, 5)), 0);  // past the vertices there are
    CHECK_EQ(int(spriteSourceOf(cells, 256, 256, bytes, 8, 16, 4)), 0);  // no texture coordinates in a vertex
    CHECK_EQ(int(spriteSourceOf({}, 256, 256, bytes, 8, 32, 4)), 0);
}

TEST(frame_sprite_cells_written_replace_what_they_cover) {
    // 0.32: a cache's cells and what each holds now; 2.2.2: a cell of another size over them (the texture taken back
    // for frames of another size) puts the old ones out - looked up by place, they named the frame's kind wrongly
    std::vector<SpriteCell> cells;
    const SpriteCell monster{Rect{0, 0, 64, 64}, 1}, item{Rect{64, 0, 128, 64}, 4};
    spriteCellsWritten(cells, &monster, 1);
    spriteCellsWritten(cells, &item, 1);
    CHECK_EQ(cells.size(), size_t(2));
    const SpriteCell npc{Rect{0, 0, 64, 64}, 3};  // the same cell, another frame
    spriteCellsWritten(cells, &npc, 1);
    CHECK(cells.size() == 2 && spriteSourceAt(cells, 256, 256, 0.1f, 0.1f) == 3);
    const SpriteCell player{Rect{32, 0, 64, 32}, 2};  // a smaller cell in the first one's place
    spriteCellsWritten(cells, &player, 1);
    CHECK_EQ(cells.size(), size_t(2));
    CHECK_EQ(int(spriteSourceAt(cells, 256, 256, 0.15f, 0.05f)), 2);
    CHECK_EQ(int(spriteSourceAt(cells, 256, 256, 0.05f, 0.05f)), 0);  // the rest of the old cell: nothing known
    CHECK_EQ(int(spriteSourceAt(cells, 256, 256, 0.3f, 0.1f)), 4);
}

// 3.0.3-beta.8: the game's work since the last present - draws, clears and writes, kept through a flush - so the
// windows' panel goes over a picture drawn anew only
TEST(frame_counts_the_work_of_a_frame) {
    FrameRecord r;
    CHECK_EQ(r.takeWork(), 0u);
    uint8_t vertices[32 * 3] = {};
    DrawCmd cmd;
    cmd.fvf = d3d7::FVF_TL1;
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    r.draw(cmd, vertices, 3, nullptr, 0);
    ClearCmd clear;
    clear.rect = Rect{0, 0, 4, 4};
    r.clear(clear);
    r.reset();  // a flush within the frame
    const uint32_t argb[4] = {};
    r.writeTarget(Rect{0, 0, 2, 2}, argb);
    CHECK_EQ(r.takeWork(), 3u);
    CHECK_EQ(r.takeWork(), 0u);
    r.upload(1, 0, Rect{0, 0, 2, 2}, argb);  // a texture's pixels are not a picture drawn
    CHECK_EQ(r.takeWork(), 0u);
}

// 3.0.3-beta.9: the windows' panel put before the game's cursor in the frame - its last commands moved before one
TEST(frame_moves_its_last_commands_before_one) {
    FrameRecord r;
    uint8_t vertices[32 * 3] = {};
    DrawCmd cmd;
    cmd.fvf = d3d7::FVF_TL1;
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    for (int i = 0; i < 3; ++i) r.draw(cmd, vertices, 3, nullptr, 0);  // the game's: 0, 1, 2
    const uint32_t argb[4] = {};
    r.upload(5, 0, Rect{0, 0, 2, 2}, argb);  // the panel's: its picture and its quad
    r.draw(cmd, vertices, 3, nullptr, 0);
    r.moveLastBefore(2, 2);
    CHECK_EQ(r.commands().size(), size_t(5));
    if (r.commands().size() != 5) return;
    CHECK(r.commands()[0].type == Cmd::Draw && r.commands()[0].index == 0);
    CHECK(r.commands()[1].type == Cmd::Draw && r.commands()[1].index == 1);
    CHECK(r.commands()[2].type == Cmd::Upload);
    CHECK(r.commands()[3].type == Cmd::Draw && r.commands()[3].index == 3);
    CHECK(r.commands()[4].type == Cmd::Draw && r.commands()[4].index == 2);  // the game's last one after them
    r.moveLastBefore(9, 0);  // more than there are: nothing moves
    CHECK(r.commands()[4].index == 2);
}

// 3.0.11-beta.102 (the concept «Контроллер в интерфейсе uaRO»: «Курсор при игре с контроллера скрыт»): a command
// dropped out of the frame (the game's cursor while the stick walks), the rest kept in their order
TEST(frame_drops_one_command) {
    FrameRecord r;
    uint8_t vertices[32 * 3] = {};
    DrawCmd cmd;
    cmd.fvf = d3d7::FVF_TL1;
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    for (int i = 0; i < 3; ++i) r.draw(cmd, vertices, 3, nullptr, 0);
    r.dropCommand(1);
    CHECK(r.commands().size() == 2 && r.commands()[0].index == 0 && r.commands()[1].index == 2);
    r.dropCommand(7);  // none there: nothing dropped
    CHECK_EQ(r.commands().size(), size_t(2));
}

// 3.0.11-beta.185 (the tester's «вода іде оверлеем навіть над меню - і моби з деревами теж»): the game's windows are
// filed before its scene begins, so the place noted then was the frame's start - the whole world went over the
// driver's windows wherever nothing upscaled it under them. The game's own windows begin at its interface's draw after
// the faces filed before them (the names over the heads, the chat's balloons): that one's place
TEST(frame_finds_an_interface_draw) {
    FrameRecord f;
    uint8_t v[4 * 32] = {};
    DrawSetup world, ui;
    ui.ui = true;
    DrawCmd d;
    d.primitive = d3d7::PT_TRIANGLESTRIP;
    d.fvf = d3d7::FVF_TL1;
    CHECK_EQ(f.interfaceDraw(0), size_t(0));  // nothing: its end
    f.clear(ClearCmd{d3d7::CLEAR_TARGET, 0xFF000000, 1.0f, Rect{0, 0, 4, 4}});
    d.state = f.addState(world);
    f.draw(d, v, 4, nullptr, 0);  // 1 the ground
    f.draw(d, v, 4, nullptr, 0);  // 2 the water, the trees, the monsters
    CHECK_EQ(f.interfaceDraw(0), size_t(3));  // the world alone: its end
    d.state = f.addState(ui);
    f.draw(d, v, 4, nullptr, 0);  // 3 a name over a head
    const uint32_t px[4] = {1, 2, 3, 4};
    f.upload(7, 0, Rect{0, 0, 2, 2}, px);  // 4 an upload between: not a draw
    f.draw(d, v, 4, nullptr, 0);  // 5 the game's window
    f.draw(d, v, 4, nullptr, 0);  // 6 its cursor
    CHECK_EQ(f.interfaceDraw(0), size_t(3));
    CHECK_EQ(f.interfaceDraw(1), size_t(5));
    CHECK_EQ(f.interfaceDraw(2), size_t(6));
    CHECK_EQ(f.interfaceDraw(3), size_t(7));  // not that many: the end
    CHECK_EQ(f.interfaceDraw(SIZE_MAX), size_t(7));
}

// 3.0.11-beta.190 (R-RCFQHR «строка хп поверх интерфейса»): the game's own windows and its cursor are its interface
// list's faces from its windows on; the depth-sorted screen list after them (the bars under the characters) stays
// where it is - under the driver's windows. The commands [from, to) taken out, the rest kept in order
TEST(frame_takes_a_range) {
    FrameRecord r;
    uint8_t vertices[32 * 3] = {};
    DrawCmd cmd;
    cmd.fvf = d3d7::FVF_TL1;
    cmd.primitive = d3d7::PT_TRIANGLELIST;
    for (int i = 0; i < 5; ++i) r.draw(cmd, vertices, 3, nullptr, 0);  // the world, a name, a window, the cursor, a bar
    std::vector<Command> mid = r.takeRange(2, 4);
    CHECK(mid.size() == 2 && r.commands().size() == 3);
    CHECK(r.commands()[0].index == 0 && r.commands()[1].index == 1 && r.commands()[2].index == 4);
    CHECK(mid[0].index == 2 && mid[1].index == 3);
    CHECK(r.takeRange(2, 99).size() == 1 && r.commands().size() == 2);  // past the end: to it
    CHECK(r.takeRange(5, 9).empty() && r.takeRange(1, 1).empty() && r.commands().size() == 2);
}

// 3.0.11-beta.294 (R-WTXHXN: a crab and a ghost over the driver's chat): the game's windows' range taken over the
// driver's interface - the world's draws within it and the uploads stay where they were, under the driver's panels
TEST(frame_takes_the_interface_alone) {
    FrameRecord f;
    uint8_t v[4 * 32] = {};
    DrawSetup world, ui;
    ui.ui = true;
    DrawCmd d;
    d.primitive = d3d7::PT_TRIANGLESTRIP;
    d.fvf = d3d7::FVF_TL1;
    const uint32_t worldState = f.addState(world), uiState = f.addState(ui);
    d.state = worldState;
    f.draw(d, v, 4, nullptr, 0);  // 0 the ground
    d.state = uiState;
    f.draw(d, v, 4, nullptr, 0);  // 1 the game's window
    d.state = worldState;
    f.draw(d, v, 4, nullptr, 0);  // 2 a monster drawn among them
    const uint32_t px[4] = {1, 2, 3, 4};
    f.upload(7, 0, Rect{0, 0, 2, 2}, px);  // 3 an upload
    d.state = uiState;
    f.draw(d, v, 4, nullptr, 0);  // 4 its cursor
    size_t kept = 99;
    std::vector<Command> game = f.takeInterface(1, 99, kept);
    CHECK_EQ(kept, size_t(1));
    CHECK(game.size() == 2 && game[0].index == 1 && game[1].index == 3);  // the window and the cursor (draws 1, 3)
    CHECK(f.commands().size() == 3 && f.commands()[1].type == Cmd::Draw && f.commands()[1].index == 2 &&
          f.commands()[2].type == Cmd::Upload);
    CHECK(f.takeInterface(5, 9, kept).empty() && kept == 0);
}

// 3.0.4-beta.5 (the user's idea 5): the photo mode - the game's interface (the draws after its depth went off: the
// windows, the text, the cursor) and the driver's own panel out of the frame, the world, its clears and uploads kept
TEST(frame_drops_the_interface) {
    FrameRecord f;
    uint8_t v[4 * 32] = {};
    DrawSetup world, ui;
    ui.ui = true;
    DrawCmd d;
    d.primitive = d3d7::PT_TRIANGLESTRIP;
    d.fvf = d3d7::FVF_TL1;
    f.clear(ClearCmd{d3d7::CLEAR_TARGET, 0xFF000000, 1.0f, Rect{0, 0, 4, 4}});
    d.state = f.addState(world);
    f.draw(d, v, 4, nullptr, 0);
    const uint32_t px[4] = {1, 2, 3, 4};
    f.upload(7, 0, Rect{0, 0, 2, 2}, px);
    d.state = f.addState(ui);
    f.draw(d, v, 4, nullptr, 0);
    d.state = f.addState(world);
    f.draw(d, v, 4, nullptr, 0);
    d.state = f.addState(ui);
    f.draw(d, v, 4, nullptr, 0);
    CHECK_EQ(f.dropInterface(), size_t(2));
    CHECK_EQ(f.commands().size(), size_t(4));  // the clear, two world draws, the upload
    size_t draws = 0;
    for (const Command& c : f.commands())
        if (c.type == Cmd::Draw) {
            ++draws;
            CHECK(!f.states()[f.draws()[c.index].state].ui);
        }
    CHECK_EQ(draws, size_t(2));
    CHECK_EQ(f.dropInterface(), size_t(0));
}

// 3.0.11-beta.49: where the world ends - the first interface draw's place (the world dimmed under the windows goes
// there)
TEST(frame_finds_the_interface) {
    FrameRecord f;
    uint8_t v[4 * 32] = {};
    DrawSetup world, ui;
    ui.ui = true;
    DrawCmd d;
    d.primitive = d3d7::PT_TRIANGLESTRIP;
    d.fvf = d3d7::FVF_TL1;
    CHECK_EQ(f.firstInterface(), size_t(0));  // nothing: its end
    f.clear(ClearCmd{d3d7::CLEAR_TARGET, 0xFF000000, 1.0f, Rect{0, 0, 4, 4}});
    d.state = f.addState(world);
    f.draw(d, v, 4, nullptr, 0);
    CHECK_EQ(f.firstInterface(), size_t(2));  // the world alone
    d.state = f.addState(ui);
    f.draw(d, v, 4, nullptr, 0);
    d.state = f.addState(world);
    f.draw(d, v, 4, nullptr, 0);
    CHECK_EQ(f.firstInterface(), size_t(2));
    f.draw(d, v, 4, nullptr, 0);  // one more drawn last, moved there: before the interface
    f.moveLastBefore(1, f.firstInterface());
    CHECK(!f.states()[f.draws()[f.commands()[2].index].state].ui && f.firstInterface() == 3);
}
