#include "core/closed_windows.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.120 (the concept «Ігровий інтерфейс», stage 2: «✕ на каждом окне»): our windows closed by ✕ - kept by
// their keys, a line each, across the game's starts; unknown words and blanks left out, each once
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
    CHECK(closedwin::ownOfKey('H', o) && o == closedwin::Own::Hunt);
    CHECK(closedwin::ownOfKey('B', o) && o == closedwin::Own::Mvp);
    CHECK(closedwin::ownOfKey('J', o) && o == closedwin::Own::Tracker);
    CHECK(closedwin::ownOfKey('G', o) && o == closedwin::Own::Party);
    CHECK(closedwin::ownOfKey('K', o) && o == closedwin::Own::Recap);
    CHECK(!closedwin::ownOfKey('F', o));  // «Довідник» keeps its own Ctrl+F
    CHECK(!closedwin::ownOfKey('A', o));
    CHECK_EQ(std::string(closedwin::keyName(closedwin::Own::Hunt)), std::string("Ctrl+H"));
}
