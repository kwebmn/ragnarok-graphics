// 3.0.11-beta.175 (the login concept's shelf): «kweb додано на полицю · Скасувати» at the screen's top middle after
// a login put on the shelf by itself (core/shelf_toast) - its places, the mouse on «Скасувати», its words
#include <algorithm>
#include <cmath>
#include <string>

#include "core/shelf_toast.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(shelf_toast_places_and_undo) {
    const std::string text = "kweb додано на полицю", undo = "Скасувати";
    const shelftoast::Layout L = shelftoast::layout(1920, 1080, 1.0f, text, undo);
    CHECK(std::abs(L.x + L.w / 2 - 960) <= 2 && L.y >= 16 && L.y + L.h < 120);
    CHECK(L.undo.x >= L.text.x + L.text.w && L.undo.x + L.undo.w <= L.w + 0.5f);
    CHECK(shelftoast::overUndo(L, L.undo.x + L.undo.w / 2, L.undo.y + L.undo.h / 2));
    CHECK(!shelftoast::overUndo(L, L.text.x + 2, L.text.y + L.text.h / 2));
    const auto w = shelftoast::words(L, text, undo, false);
    CHECK(std::any_of(w.begin(), w.end(), [&](const shelftoast::Words& x) { return x.text == text; }));
    CHECK(std::any_of(w.begin(), w.end(), [&](const shelftoast::Words& x) { return x.text == undo; }));
    CHECK(shelftoast::kShowMs == 6000);
    panel::Picture p;
    p.width = L.w;
    p.height = L.h;
    p.px.assign(size_t(L.w) * size_t(L.h), 0u);
    shelftoast::paint(p, L, false);
    CHECK((p.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);  // its ground
}
