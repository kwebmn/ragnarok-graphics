// 3.0.11-beta.205 (the tester's third list: «alt l - список емоцій 1) в старому стилі 2) ... по даблкліку теж
// виконувались»): the game's emotion list (UI window 0x57) as a card (core/emote_card) - its 49 emotions in a grid,
// each the game's own picture over its command
#include <algorithm>
#include <string>
#include <vector>

#include "core/emote_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

emotecard::Texts texts() {
    emotecard::Texts t;
    t.title = "Емоції";
    t.hint = "Клік - у чат, подвійний - одразу";
    return t;
}

std::vector<std::string> commands() {
    std::vector<std::string> c;
    for (int i = 0; i < emotecard::kEmotions; ++i) c.push_back("/e" + std::to_string(i));
    return c;
}

bool has(const std::vector<emotecard::Words>& w, const std::string& text) {
    return std::any_of(w.begin(), w.end(), [&](const emotecard::Words& x) { return x.text == text; });
}

}  // namespace

// Seven a row, all 49 inside the card and apart; the card at the screen's middle across, inside the screen
TEST(emote_card_grid) {
    const emotecard::Layout L = emotecard::layout(1920, 1080, 1.0f);
    CHECK_EQ(int(L.cells.size()), emotecard::kEmotions);
    CHECK_EQ(L.x, 1920 / 2 - L.w / 2);
    CHECK(L.y >= 0 && L.y + L.h <= 1080);
    CHECK(L.cells[0].y == L.cells[6].y && L.cells[7].y > L.cells[0].y);
    CHECK(L.cells[7].x == L.cells[0].x);
    for (size_t i = 0; i < L.cells.size(); ++i) {
        const panel::Box& c = L.cells[i];
        CHECK(c.x >= 0 && c.y >= 0 && c.x + c.w <= float(L.w) && c.y + c.h <= float(L.h));
        if (i + 1 < L.cells.size() && (i + 1) % emotecard::kColumns != 0) CHECK(c.x + c.w <= L.cells[i + 1].x);
    }
    const emotecard::Layout small = emotecard::layout(800, 600, 1.0f);
    CHECK(small.y >= 0 && small.y + small.h <= 600);
    const emotecard::Layout big = emotecard::layout(3840, 2160, 2.0f);
    CHECK(big.w >= 2 * L.w - 2);
}

// A cell found where it is drawn; between the cells and outside none; ✕ at the top right
TEST(emote_card_hits) {
    const emotecard::Layout L = emotecard::layout(1920, 1080, 1.0f);
    for (int i : {0, 6, 7, 24, 48}) {
        const panel::Box& c = L.cells[size_t(i)];
        CHECK_EQ(emotecard::cellAt(L, c.x + c.w / 2, c.y + c.h / 2), i);
    }
    CHECK_EQ(emotecard::cellAt(L, 2, 2), -1);
    CHECK_EQ(emotecard::cellAt(L, -5, 40), -1);
    CHECK(emotecard::closeAt(L, L.close.x + L.close.w / 2, L.close.y + L.close.h / 2));
    CHECK(!emotecard::closeAt(L, L.cells[0].x + 2, L.cells[0].y + 2));
    CHECK(L.close.x > float(L.w) / 2 && L.close.y < L.cells[0].y);
}

// The title, the hint, each emotion's command under it; the hovered one's in gold
TEST(emote_card_words) {
    const emotecard::Layout L = emotecard::layout(1920, 1080, 1.0f);
    emotecard::Show sh;
    sh.hover = 3;
    const std::vector<emotecard::Words> w = emotecard::words(L, commands(), sh, texts());
    CHECK(has(w, "Емоції"));
    CHECK(has(w, "Клік - у чат, подвійний - одразу"));
    CHECK(has(w, "/e0") && has(w, "/e48"));
    const auto e3 = std::find_if(w.begin(), w.end(), [](const emotecard::Words& x) { return x.text == "/e3"; });
    const auto e4 = std::find_if(w.begin(), w.end(), [](const emotecard::Words& x) { return x.text == "/e4"; });
    CHECK(e3 != w.end() && e4 != w.end() && e3->rgb != e4->rgb);
    CHECK(e3 != w.end() && e3->box.y >= L.cells[3].y && e3->box.y + e3->box.h <= L.cells[3].y + L.cells[3].h + 0.5f);
    // fewer commands than cells (another build's list): no words for the rest
    const std::vector<emotecard::Words> few = emotecard::words(L, {"/!", "/?"}, emotecard::Show{}, texts());
    CHECK(has(few, "/?") && !has(few, "/e5"));
}

// The game's pictures in their cells: a picture's middle there, a cell with none left plain
TEST(emote_card_icons_painted) {
    const emotecard::Layout L = emotecard::layout(1920, 1080, 1.0f);
    panel::Picture icon;
    icon.width = icon.height = 10;
    icon.px.assign(100, 0xFFFF0000);
    std::vector<panel::Picture> icons(size_t(emotecard::kEmotions));
    icons[5] = icon;
    panel::Picture out;
    CHECK(panel::blank(out, L.w, L.h));
    emotecard::paint(out, L, emotecard::Show{}, icons);
    const panel::Box& c = L.icons[5];
    const uint32_t mid = out.px[size_t(int(c.y + c.h / 2)) * size_t(out.width) + size_t(int(c.x + c.w / 2))];
    CHECK_EQ(mid, 0xFFFF0000u);
    const panel::Box& d = L.icons[6];
    const uint32_t other = out.px[size_t(int(d.y + d.h / 2)) * size_t(out.width) + size_t(int(d.x + d.w / 2))];
    CHECK(other != 0xFFFF0000u);
}
