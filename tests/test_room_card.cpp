// 3.0.11-beta.208 (the tester's third list: «alt c - вже створений чат - в старому стилі»): the game's chat room window
// (UI window 0x1C) as a card (core/room_card) - its title, its lines, its members (the owner marked), the game's own
// line to type into, the owner's tools
#include <algorithm>
#include <string>
#include <vector>

#include "core/room_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

roomcard::Texts texts() {
    roomcard::Texts t;
    t.open = "Відкрита";
    t.locked = "З паролем";
    t.members = "Учасники";
    t.setup = "Налаштування";
    t.leave = "Вийти";
    t.kick = "Вигнати";
    t.owner = "Зробити власником";
    t.friendAdd = "У друзі";
    return t;
}

roomcard::Card card() {
    roomcard::Card c;
    c.title = "Торгую";
    c.open = true;
    c.count = 2;
    c.limit = 20;
    c.members = {{"Ann", true}, {"Bob", false}};
    for (int i = 0; i < 30; ++i) c.lines.push_back(roomcard::Line{"line " + std::to_string(i), 0});
    return c;
}

bool has(const std::vector<roomcard::Words>& w, const std::string& text) {
    return std::any_of(w.begin(), w.end(), [&](const roomcard::Words& x) { return x.text == text; });
}

}  // namespace

// The game's colours (0x00BBGGRR) as the card's (0xRRGGBB)
TEST(room_card_colours) {
    CHECK_EQ(roomcard::rgbOf(0x00FF0000), 0x0000FFu);
    CHECK_EQ(roomcard::rgbOf(0x000000FF), 0xFF0000u);
    CHECK_EQ(roomcard::rgbOf(0x00808000), 0x008080u);
}

// The parts where they are drawn; the owner's tools only for the owner, a member's actions only when one is chosen
TEST(room_card_layout_and_hits) {
    roomcard::Card c = card();
    roomcard::Layout L = roomcard::layout(c, 1920, 1080, 1.0f);
    CHECK(L.x >= 0 && L.y >= 0 && L.x + L.w <= 1920 && L.y + L.h <= 1080);
    CHECK_EQ(int(L.memberRows.size()), 2);
    CHECK(L.lineRows >= 8);
    CHECK(L.setup.w == 0 && L.kick.w == 0 && L.friendAdd.w == 0);
    using P = roomcard::Part;
    const auto at = [&](const panel::Box& b) { return roomcard::hitTest(L, b.x + b.w / 2, b.y + b.h / 2); };
    CHECK(at(L.memberRows[1]).part == P::Member && at(L.memberRows[1]).index == 1);
    CHECK(at(L.input).part == P::Input);
    CHECK(at(L.leave).part == P::Leave);
    CHECK(at(L.close).part == P::Close);
    c.mine = true;
    c.chosen = 1;
    L = roomcard::layout(c, 1920, 1080, 1.0f);
    CHECK(L.setup.w > 0 && L.kick.w > 0 && L.owner.w > 0 && L.friendAdd.w > 0);
    CHECK(at(L.setup).part == P::Setup && at(L.kick).part == P::Kick && at(L.owner).part == P::Owner);
    CHECK(at(L.friendAdd).part == P::Friend);
    c.mine = false;
    L = roomcard::layout(c, 1920, 1080, 1.0f);
    CHECK(L.setup.w == 0 && L.kick.w == 0 && L.owner.w == 0 && L.friendAdd.w > 0);
}

