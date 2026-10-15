#include <vector>

#include "core/ai/cell_grid.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::ai;

namespace {

// A 128x128 texture with a frame of colour c in the cell (cx, cy) of 32x64 cells.
void paint(std::vector<uint32_t>& t, int cx, int cy, uint32_t c) {
    for (int y = cy * 64; y < cy * 64 + 50; ++y)
        for (int x = cx * 32; x < cx * 32 + 20; ++x) t[size_t(y) * 128 + size_t(x)] = c;
}

}  // namespace

TEST(cell_grid_finds_frames_the_game_wrote_past_its_call) {
    // 0.49.3: a frame cached before the driver saw the texture to be a sprite cache, or written another way, is a
    // whole cell that changed; the cells the game said it wrote are not told again.
    CellGrid g;
    g.reset(128, 128);
    std::vector<uint32_t> t(128 * 128, 0);
    paint(t, 1, 0, 0xFF112233u);  // there before
    paint(t, 2, 1, 0xFF445566u);  // written through the call
    const SpriteCell written{Rect{64, 64, 96, 128}, 2};
    g.noteCell(written.rect);
    CHECK(g.cellWidth() == 32 && g.cellHeight() == 64);
    std::vector<Rect> cells;
    Rect rest;
    g.update(Rect{0, 0, 128, 128}, t.data(), &written, 1, cells, rest);
    CHECK(cells.size() == 1);
    CHECK(cells[0] == (Rect{32, 0, 64, 64}));
    CHECK(rest.empty());
    g.update(Rect{0, 0, 128, 128}, t.data(), nullptr, 0, cells, rest);  // nothing changed since
    CHECK(cells.empty() && rest.empty());
    paint(t, 3, 1, 0xFF778899u);  // another way, in a piece of the texture
    std::vector<uint32_t> piece(64 * 64);
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) piece[size_t(y) * 64 + size_t(x)] = t[size_t(y + 64) * 128 + size_t(x + 64)];
    g.update(Rect{64, 64, 128, 128}, piece.data(), nullptr, 0, cells, rest);
    CHECK(cells.size() == 1);
    CHECK(cells[0] == (Rect{96, 64, 128, 128}));
    CHECK(rest.empty());
}

TEST(cell_grid_without_a_cell_size_tells_the_changed_bounds) {
    CellGrid g;
    g.reset(64, 64);
    std::vector<uint32_t> t(64 * 64, 0);
    t[10 * 64 + 5] = 0xFF000001u;
    t[20 * 64 + 30] = 0xFF000002u;
    std::vector<Rect> cells;
    Rect rest;
    g.update(Rect{0, 0, 64, 64}, t.data(), nullptr, 0, cells, rest);
    CHECK(cells.empty());
    CHECK(rest == (Rect{5, 10, 31, 21}));
    g.noteCell(Rect{3, 0, 35, 64});  // no cell of a grid from the corner: not taken
    CHECK(g.cellWidth() == 0);
    g.update(Rect{0, 0, 64, 64}, nullptr, nullptr, 0, cells, rest);  // nothing to compare
    CHECK(cells.empty() && rest.empty());
    g.update(Rect{0, 0, 65, 64}, t.data(), nullptr, 0, cells, rest);  // past the texture
    CHECK(cells.empty() && rest.empty());
}

TEST(cell_grid_cleared_pieces) {
    std::vector<uint32_t> t(16, 0);
    CHECK(cleared(t.data(), 4, 4, 4));
    t[15] = 1;
    CHECK(!cleared(t.data(), 4, 4, 4));
    CHECK(cleared(t.data(), 3, 3, 4));
}
