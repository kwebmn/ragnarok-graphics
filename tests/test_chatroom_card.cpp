// 3.0.11-beta.207 (the tester's third list: «alt c - створити чат рум - в старому стилі»): the game's chat room
// making window (UI window 0x1B) as a card (core/chatroom_card) - the game's own title and password fields, the
// places as chips, open or closed, «Створити» and «Скасувати»
#include <algorithm>
#include <string>
#include <vector>

#include "core/chatroom_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

chatroomcard::Texts texts() {
    chatroomcard::Texts t;
    t.heading = "Нова чат-кімната";
    t.titleLabel = "Назва";
    t.limitLabel = "Місць";
    t.open = "Відкрита";
    t.closed = "З паролем";
    t.passwordLabel = "Пароль";
    t.create = "Створити";
    t.cancel = "Скасувати";
    return t;
}

bool has(const std::vector<chatroomcard::Words>& w, const std::string& text) {
    return std::any_of(w.begin(), w.end(), [&](const chatroomcard::Words& x) { return x.text == text; });
}

}  // namespace

// Each part found where it is drawn: the fields, a chip per place count, open and closed, the buttons, ✕
TEST(chatroom_card_layout_and_hits) {
    const chatroomcard::Layout L = chatroomcard::layout(6, 1920, 1080, 1.0f);
    CHECK_EQ(int(L.limits.size()), 6);
    CHECK_EQ(L.x, 1920 / 2 - L.w / 2);
    CHECK(L.y >= 0 && L.y + L.h <= 1080);
    using P = chatroomcard::Part;
    const auto at = [&](const panel::Box& b) { return chatroomcard::hitTest(L, b.x + b.w / 2, b.y + b.h / 2); };
    CHECK(at(L.title).part == P::Title);
    CHECK(at(L.password).part == P::Password);
    CHECK(at(L.limits[3]).part == P::Limit && at(L.limits[3]).index == 3);
    CHECK(at(L.open).part == P::Open && at(L.closed).part == P::Closed);
    CHECK(at(L.create).part == P::Create && at(L.cancel).part == P::Cancel);
    CHECK(at(L.close).part == P::Close);
    CHECK(chatroomcard::hitTest(L, 1, 1).part == P::None);
    CHECK(L.title.y < L.limits[0].y && L.limits[0].y < L.open.y && L.open.y < L.password.y &&
          L.password.y < L.create.y);
}

