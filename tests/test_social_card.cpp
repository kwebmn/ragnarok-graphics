// 3.0.11-beta.163 (tasks 5 and 6 of the remaining windows: the friends and the party, Alt+H / the party's button): the
// game's friends and party window (UI window 0x22, its two tabs) as a card (core/social_card) - a row a person (online
// or not, the party's leader, where they are), the chosen one's actions the window's own buttons
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "core/social_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

std::pair<float, float> mid(const panel::Box& b) { return {b.x + b.w / 2, b.y + b.h / 2}; }

}  // namespace

// The window's own (FUN_004c2250): the tab's choice (6 / 0x95 with 1 the friends, 2 the party), the whisper 0xa6, a
// friend deleted / a member expelled 0xa9, the party left 0xaa (with one's own row chosen), the settings 0xa8, ✕ 0x87
TEST(social_card_the_windows_buttons) {
    using A = socialcard::Action;
    CHECK(socialcard::kTabMsg == 0x95 && socialcard::tabParam(socialcard::Tab::Friends) == 1);
    CHECK(socialcard::tabParam(socialcard::Tab::Party) == 2);
    CHECK(socialcard::gameButton(A::Whisper) == 0xa6 && socialcard::gameButton(A::Remove) == 0xa9);
    CHECK(socialcard::gameButton(A::Leave) == 0xaa && socialcard::gameButton(A::Settings) == 0xa8);
    CHECK(socialcard::gameButton(A::Close) == 0x87);
}

// Which actions each tab shows: the friends - whisper, delete, settings; the party - whisper, expel, leave, settings;
// a person's actions only with one chosen, not on oneself (but leaving)
TEST(social_card_actions_by_tab_and_choice) {
    using A = socialcard::Action;
    using T = socialcard::Tab;
    CHECK((socialcard::actions(T::Friends) == std::vector<A>{A::Whisper, A::Remove, A::Settings}));
    CHECK((socialcard::actions(T::Party) == std::vector<A>{A::Whisper, A::Remove, A::Leave, A::Settings}));
    socialcard::Entry other, me;
    other.name = "Ann";
    me.name = "Me";
    me.self = true;
    CHECK(socialcard::allowed(T::Party, A::Whisper, &other) && !socialcard::allowed(T::Party, A::Whisper, &me));
    CHECK(socialcard::allowed(T::Party, A::Remove, &other) && !socialcard::allowed(T::Party, A::Remove, &me));
    CHECK(!socialcard::allowed(T::Party, A::Whisper, nullptr) && socialcard::allowed(T::Party, A::Leave, nullptr));
    CHECK(socialcard::allowed(T::Friends, A::Settings, nullptr) && !socialcard::allowed(T::Friends, A::Leave, &other));
}

TEST(social_card_places_and_the_mouse) {
    const socialcard::Layout L = socialcard::layout(1920, 1080, 1.0f, socialcard::Tab::Party, 3);
    CHECK(L.rows.size() == 3 && L.buttons.size() == 4);
    CHECK(L.tabs[1].x > L.tabs[0].x && L.rows[0].y > L.tabs[0].y && L.buttons[0].y > L.rows[2].y);
    using P = socialcard::Part;
    const socialcard::Hit t = socialcard::hitTest(L, 0, 3, mid(L.tabs[1]).first, mid(L.tabs[1]).second);
    CHECK(t.part == P::Tab && t.index == 1);
    const socialcard::Hit r = socialcard::hitTest(L, 0, 3, mid(L.rows[2]).first, mid(L.rows[2]).second);
    CHECK(r.part == P::Row && r.index == 2);
    const socialcard::Hit b = socialcard::hitTest(L, 0, 3, mid(L.buttons[3]).first, mid(L.buttons[3]).second);
    CHECK(b.part == P::Button && b.index == 3);
    CHECK(socialcard::hitTest(L, 0, 3, mid(L.close).first, mid(L.close).second).part == P::Close);
    const socialcard::Layout many = socialcard::layout(1920, 1080, 1.0f, socialcard::Tab::Friends, 30);
    CHECK(many.rows.size() == 10 && socialcard::mostScroll(many, 30) == 20 && many.buttons.size() == 3);
}

// 3.0.18-beta.141 (the owner: «все» - the card made handier): the friends online first (each kind in the game's own
// order), the party as the game lists it; the count online
TEST(social_card_friends_online_first) {
    std::vector<socialcard::Entry> e(4);
    e[0].name = "A", e[1].name = "B", e[2].name = "C", e[3].name = "D";
    e[1].online = e[3].online = true;
    const std::vector<int> f = socialcard::order(e, socialcard::Tab::Friends);
    CHECK(f == std::vector<int>({1, 3, 0, 2}));
    const std::vector<int> p = socialcard::order(e, socialcard::Tab::Party);
    CHECK(p == std::vector<int>({0, 1, 2, 3}));
    CHECK_EQ(socialcard::onlineCount(e), 2);
}

// A person's menu: the whisper to one online, the leader's expel, one's own leave, the friend's remove, the party
// member added to friends, the way to one elsewhere, the name copied
TEST(social_card_person_menu) {
    using socialcard::Item;
    socialcard::Entry me, other;
    me.self = me.online = true;
    other.online = true;
    other.name = "Sage";
    auto has = [](const std::vector<Item>& v, Item i) { return std::find(v.begin(), v.end(), i) != v.end(); };
    std::vector<Item> m = socialcard::menu(socialcard::Tab::Party, other, true, true);
    CHECK(has(m, Item::Whisper) && has(m, Item::Expel) && has(m, Item::AddFriend) && has(m, Item::Route) &&
          has(m, Item::CopyName));
    CHECK(!has(m, Item::Leave) && !has(m, Item::RemoveFriend));
    m = socialcard::menu(socialcard::Tab::Party, other, false, false);  // not the leader, the member here
    CHECK(!has(m, Item::Expel) && !has(m, Item::Route));
    m = socialcard::menu(socialcard::Tab::Party, me, true, false);
    CHECK(has(m, Item::Leave) && !has(m, Item::Whisper) && !has(m, Item::Expel) && !has(m, Item::AddFriend));
    other.online = false;
    m = socialcard::menu(socialcard::Tab::Friends, other, false, false);
    CHECK(has(m, Item::RemoveFriend) && !has(m, Item::Whisper) && !has(m, Item::AddFriend) && has(m, Item::CopyName));
}
