// 3.0.11-beta.90 (the concept «Контроллер в интерфейсе uaRO», stage 6): Start on the controller opens a tile of the
// game's windows - nine in three rows, the chosen one in gold, the buttons' hints under them (core/pad_tiles)
#include <algorithm>
#include <string>
#include <vector>

#include "core/pad_tiles.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(pad_tiles_layout_and_moves) {
    const padtiles::Layout L = padtiles::layout(960, 540, 1.0f);
    CHECK_EQ(L.tiles.size(), size_t(padtiles::kTiles));
    CHECK(L.x + L.w / 2 == 960 && L.y + L.h / 2 == 540);
    CHECK(L.tiles[1].x > L.tiles[0].x + L.tiles[0].w && L.tiles[3].y > L.tiles[0].y + L.tiles[0].h);
    const panel::Box& t4 = L.tiles[4];
    CHECK_EQ(padtiles::hitTest(L, t4.x + t4.w / 2, t4.y + t4.h / 2), 4);
    CHECK_EQ(padtiles::hitTest(L, 1, 1), -1);
    CHECK_EQ(padtiles::moved(0, -1, 0), 2);  // round its row
    CHECK_EQ(padtiles::moved(0, 0, -1), 6);  // round its column
    CHECK_EQ(padtiles::moved(4, 1, 1), 8);
}

TEST(pad_tiles_picture_and_words) {
    const padtiles::Layout L = padtiles::layout(300, 300, 1.0f);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    padtiles::paint(p, L, 4, false);
    const auto edge = [&](int i) {
        const panel::Box& b = L.tiles[size_t(i)];
        return p.px[size_t(int(b.y + b.h / 2)) * size_t(p.width) + size_t(int(b.x))];
    };
    CHECK(((edge(4) >> 16) & 0xFF) > ((edge(0) >> 16) & 0xFF) + 40);  // the chosen one edged gold
    std::vector<std::string> names;
    for (int i = 0; i < padtiles::kTiles; ++i) names.push_back("Tile " + std::to_string(i));
    const std::vector<padtiles::Words> w = padtiles::words(L, names, "Меню", "Відкрити", "Закрити", false);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const padtiles::Words& x) { return x.text == text; });
    };
    CHECK(has("Tile 0") && has("Tile 8") && has("Меню") && has("Відкрити") && has("Закрити") && has("A") && has("B"));
}
