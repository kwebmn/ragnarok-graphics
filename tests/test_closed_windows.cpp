#include "core/closed_windows.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.120 (the concept «Ігровий інтерфейс», stage 2: «✕ на каждом окне»): our
// windows closed by ✕ - kept by their keys, a line each, across the game's starts; unknown words and blanks left out,
// each once
TEST(closed_windows_read_and_written) {
    const closedwin::Set s = closedwin::read("hunt\r\n\nmvp\nnot-a-window\nhunt\n");
    CHECK(closedwin::has(s, closedwin::Window::Hunt));
    CHECK(closedwin::has(s, closedwin::Window::Mvp));
    CHECK(!closedwin::has(s, closedwin::Window::Party));
    CHECK_EQ(closedwin::write(s), std::string("hunt\nmvp\n"));
    closedwin::Set t = s;
    closedwin::set(t, closedwin::Window::Mvp, false);
    closedwin::set(t, closedwin::Window::Party, true);
    CHECK_EQ(closedwin::write(t), std::string("hunt\nparty\n"));
    CHECK_EQ(std::string(closedwin::keyOf(closedwin::Window::Target)), std::string("target"));
    CHECK(closedwin::read("").none());
}

// 3.0.18-beta.120 (stage 3: the character block's second row): our windows' icons in the concept's order, only those
// whose switches are on; their keys Ctrl+F/H/B/J/G/K (uaRO.exe FUN_004d96e0 takes G, H, J with Alt alone; B, K never)
TEST(closed_windows_own_row_and_keys) {
    closedwin::Switches on;
    on.reference = on.hunt = on.mvp = on.tracker = on.party = on.recap = true;
    const std::vector<closedwin::Own> all = closedwin::row(on);
    CHECK_EQ(all.size(), size_t(7));
    CHECK(all.front() == closedwin::Own::Reference && all.back() == closedwin::Own::Settings);
    closedwin::Switches some;
    some.hunt = true;
    const std::vector<closedwin::Own> few = closedwin::row(some);
    CHECK(few.size() == 2 && few[0] == closedwin::Own::Hunt && few[1] == closedwin::Own::Settings);
    closedwin::Own o{};
    const closedwin::Keys& k = closedwin::kDefaultKeys;
    CHECK(closedwin::ownOfKey('H', k, o) && o == closedwin::Own::Hunt);
    CHECK(closedwin::ownOfKey('B', k, o) && o == closedwin::Own::Mvp);
    CHECK(closedwin::ownOfKey('J', k, o) && o == closedwin::Own::Tracker);
    CHECK(closedwin::ownOfKey('G', k, o) && o == closedwin::Own::Party);
    CHECK(closedwin::ownOfKey('K', k, o) && o == closedwin::Own::Recap);
    CHECK(!closedwin::ownOfKey('F', k, o));  // «Довідник» keeps its own Ctrl+F
    CHECK(!closedwin::ownOfKey('A', k, o));
    CHECK_EQ(closedwin::keyName(closedwin::Own::Hunt, k), std::string("Ctrl+H"));
    CHECK_EQ(closedwin::keyName(closedwin::Own::Reference, k), std::string("Ctrl+F"));
    // 3.0.18-beta.125 (item 2: «Клавіші»): the player's own letters; none - no key
    closedwin::Keys mine = k;
    mine[size_t(closedwin::Own::Hunt)] = 'U';
    mine[size_t(closedwin::Own::Recap)] = 0;
    CHECK(closedwin::ownOfKey('U', mine, o) && o == closedwin::Own::Hunt);
    CHECK(!closedwin::ownOfKey('H', mine, o));
    CHECK(!closedwin::ownOfKey('K', mine, o));
    CHECK_EQ(closedwin::keyName(closedwin::Own::Recap, mine), std::string());
}
