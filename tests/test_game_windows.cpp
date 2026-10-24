// 3.0.3-beta.6: the game windows as they tell each other - the live ones, Ctrl+Tab's previous one, the keys taken
// in the game window (core/game_windows)
#include <string>
#include <vector>

#include "core/game_windows.hpp"
#include "test.hpp"

using namespace uagfx;
using gamewindows::Action;

namespace {

gamewindows::Entry entry(uint32_t pid, int number, uint64_t frontAt, const char* name) {
    gamewindows::Entry e{};
    e.pid = pid;
    e.window = 0x1000 + pid;
    e.number = uint32_t(number);
    e.frontAt = frontAt;
    gamewindows::putName(e, name);
    e.baseLevel = 99;
    e.jobLevel = 70;
    e.hp = 85;
    e.sp = 40;
    return e;
}

std::vector<gamewindows::Window> three(uint64_t lena, uint64_t prist, uint64_t linker) {
    gamewindows::Table t{};
    t.entries[0] = entry(10, 1, lena, "Lena");
    t.entries[1] = entry(20, 2, prist, "Prist");
    t.entries[2] = entry(30, 3, linker, "Linker");
    return gamewindows::windows(t, [](const gamewindows::Entry&) { return true; });
}

const bool kCtrl = true, kAlt = true, kShift = true;

}  // namespace

TEST(game_windows_live_ones_by_number) {
    gamewindows::Table t{};
    t.entries[2] = entry(30, 3, 0, "Linker");
    t.entries[0] = entry(10, 1, 500, "Lena");
    t.entries[1] = entry(20, 2, 400, "Prist");
    t.entries[4] = entry(50, 5, 0, "Gone");
    const auto all = gamewindows::windows(t, [](const gamewindows::Entry& e) { return e.pid != 50; });
    CHECK_EQ(all.size(), size_t(3));
    if (all.size() != 3) return;
    CHECK_EQ(all[0].number, 1);
    CHECK_EQ(all[0].name, std::string("Lena"));
    CHECK_EQ(all[1].number, 2);
    CHECK_EQ(all[2].number, 3);
    CHECK_EQ(all[2].window, uint32_t(0x1000 + 30));
    CHECK_EQ(all[2].hp, 85);
    CHECK_EQ(all[2].jobLevel, 70);
}

TEST(game_windows_an_entry_out_of_its_place_is_none) {
    gamewindows::Table t{};
    t.entries[0] = entry(10, 2, 0, "Lena");  // window 2 in the first place: not written by a game of ours
    t.entries[1] = entry(20, 0, 0, "Prist");
    t.entries[2] = entry(0, 3, 0, "Free");  // no process: a free place
    CHECK(gamewindows::windows(t, [](const gamewindows::Entry&) { return true; }).empty());
}

TEST(game_windows_a_name_without_its_end_is_cut) {
    gamewindows::Table t{};
    t.entries[0] = entry(10, 1, 0, "");
    for (size_t i = 0; i < gamewindows::kName; ++i) t.entries[0].name[i] = 'x';
    const auto all = gamewindows::windows(t, [](const gamewindows::Entry&) { return true; });
    CHECK_EQ(all.size(), size_t(1));
    if (!all.empty()) CHECK_EQ(all[0].name, std::string(gamewindows::kName - 1, 'x'));
}

TEST(game_windows_a_name_is_cut_at_a_whole_letter) {
    gamewindows::Entry e{};
    gamewindows::putName(e, "Lena");
    CHECK_EQ(std::string(e.name), std::string("Lena"));
    gamewindows::putName(e, std::string(40, 'a'));
    CHECK_EQ(std::string(e.name), std::string(31, 'a'));
    std::string cyrillic;
    for (int i = 0; i < 16; ++i) cyrillic += "\xd0\x96";  // 16 letters of two bytes: 15 fit before the end
    gamewindows::putName(e, cyrillic);
    CHECK_EQ(std::string(e.name).size(), size_t(30));
    gamewindows::putName(e, "Bo");
    CHECK_EQ(std::string(e.name), std::string("Bo"));
}

TEST(game_windows_previous_is_the_one_in_front_last) {
    const auto all = three(500, 900, 700);
    const gamewindows::Window* p = gamewindows::previous(all, 20);
    CHECK(p && p->number == 3);  // from Prist: Linker was in front after Lena
    p = gamewindows::previous(all, 10);
    CHECK(p && p->number == 2);
}

TEST(game_windows_previous_never_in_front_is_the_next_one) {
    auto all = three(0, 0, 0);
    const gamewindows::Window* p = gamewindows::previous(all, 10);
    CHECK(p && p->number == 2);
    p = gamewindows::previous(all, 30);
    CHECK(p && p->number == 1);  // after the last, the first
    all = three(0, 0, 400);
    p = gamewindows::previous(all, 10);
    CHECK(p && p->number == 3);  // the one ever in front before the ones never
}

TEST(game_windows_previous_alone_is_none) {
    gamewindows::Table t{};
    t.entries[0] = entry(10, 1, 500, "Lena");
    const auto all = gamewindows::windows(t, [](const gamewindows::Entry&) { return true; });
    CHECK(gamewindows::previous(all, 10) == nullptr);
}

TEST(game_windows_by_number) {
    const auto all = three(0, 0, 0);
    const gamewindows::Window* w = gamewindows::byNumber(all, 2);
    CHECK(w && w->name == "Prist");
    CHECK(gamewindows::byNumber(all, 7) == nullptr);
}

TEST(game_windows_keys_back_and_numbers) {
    const WindowKeys all = WindowKeys::All;
    CHECK(gamewindows::keyAction(gamewindows::kTab, kCtrl, !kAlt, !kShift, all).action == Action::Back);
    CHECK(gamewindows::keyAction(gamewindows::kTab, kCtrl, !kAlt, kShift, all).action == Action::Back);
    CHECK(gamewindows::keyAction(gamewindows::kTab, kCtrl, kAlt, !kShift, all).action == Action::None);  // Windows'
    CHECK(gamewindows::keyAction(gamewindows::kTab, !kCtrl, !kAlt, !kShift, all).action == Action::None);
    gamewindows::KeyAction a = gamewindows::keyAction('3', kCtrl, kAlt, !kShift, all);
    CHECK(a.action == Action::Window && a.number == 3);
    a = gamewindows::keyAction(gamewindows::kPad0 + 8, kCtrl, kAlt, !kShift, all);
    CHECK(a.action == Action::Window && a.number == 8);
    CHECK(gamewindows::keyAction('9', kCtrl, kAlt, !kShift, all).action == Action::None);
    CHECK(gamewindows::keyAction('0', kCtrl, kAlt, !kShift, all).action == Action::Panel);
    CHECK(gamewindows::keyAction(gamewindows::kPad0, kCtrl, kAlt, !kShift, all).action == Action::Panel);
}

TEST(game_windows_keys_the_games_own_are_left) {
    const WindowKeys all = WindowKeys::All;
    CHECK(gamewindows::keyAction('1', !kCtrl, kAlt, !kShift, all).action == Action::None);  // a chat macro
    CHECK(gamewindows::keyAction('1', kCtrl, !kAlt, !kShift, all).action == Action::None);  // an emotion
    CHECK(gamewindows::keyAction('1', kCtrl, kAlt, kShift, all).action == Action::None);
    CHECK(gamewindows::keyAction(0x70, kCtrl, kAlt, !kShift, all).action == Action::None);  // F1: the hotkey bar
}

TEST(game_windows_keys_as_the_setting_says) {
    CHECK(gamewindows::keyAction('3', kCtrl, kAlt, !kShift, WindowKeys::Back).action == Action::None);
    CHECK(gamewindows::keyAction('0', kCtrl, kAlt, !kShift, WindowKeys::Back).action == Action::None);
    CHECK(gamewindows::keyAction(gamewindows::kTab, kCtrl, !kAlt, !kShift, WindowKeys::Back).action ==
          Action::Back);
    CHECK(gamewindows::keyAction(gamewindows::kTab, kCtrl, !kAlt, !kShift, WindowKeys::Off).action == Action::None);
    CHECK(gamewindows::keyAction('3', kCtrl, kAlt, !kShift, WindowKeys::Off).action == Action::None);
}

TEST(game_windows_the_characters_numbers_in_its_entry) {
    gamewindows::Entry e{};
    charstatus::Status s;
    s.baseLevel = 99;
    s.jobLevel = 70;
    s.hp = 3;
    s.hpMost = 10000;  // a little left: 1 %, not none
    s.sp = 40;
    s.spMost = 100;
    gamewindows::putStatus(e, &s);
    CHECK_EQ(int(e.baseLevel), 99);
    CHECK_EQ(int(e.jobLevel), 70);
    CHECK_EQ(int(e.hp), 1);
    CHECK_EQ(int(e.sp), 40);
    s.hp = 0;
    s.sp = 150;  // over its most for a moment
    gamewindows::putStatus(e, &s);
    CHECK_EQ(int(e.hp), 0);
    CHECK_EQ(int(e.sp), 100);
    s.spMost = 0;  // not had yet
    s.baseLevel = 1000;
    gamewindows::putStatus(e, &s);
    CHECK_EQ(int(e.sp), -1);
    CHECK_EQ(int(e.baseLevel), 0);
    gamewindows::putStatus(e, nullptr);  // another build of the exe: nothing known
    CHECK(e.baseLevel == 0 && e.jobLevel == 0 && e.hp == -1 && e.sp == -1);
}
