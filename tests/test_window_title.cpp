// 3.0.3-beta.5: the game window's title - the character, its numbers, the map, the client (core/window_title)
#include <string>

#include "core/window_title.hpp"
#include "test.hpp"

using namespace uagfx;
using settings::Lang;

namespace {

charstatus::Status lena() {
    charstatus::Status s;
    s.baseLevel = 99;
    s.jobLevel = 70;
    s.baseExp = 4528;
    s.baseNext = 10000;
    s.jobExp = 120;
    s.jobNext = 1000;
    s.hp = 850;
    s.hpMost = 1000;
    s.sp = 40;
    s.spMost = 100;
    s.weight = 100;
    s.weightMost = 1000;
    return s;
}

}  // namespace

TEST(window_title_shows_the_character_and_the_client) {
    const charstatus::Status s = lena();
    CHECK_EQ(window_title::title("Lena", &s, "prt_fild08", "uaRO HD", Lang::Uk),
             std::string("Lena · 99/70 · 45,2 % · HP 85 % · SP 40 % · prt_fild08 - uaRO HD"));
    CHECK_EQ(window_title::title("Lena", &s, "prt_fild08", "uaRO", Lang::En),
             std::string("Lena · 99/70 · 45.2 % · HP 85 % · SP 40 % · prt_fild08 - uaRO"));
}

TEST(window_title_no_character_the_client_alone) {
    const charstatus::Status s = lena();
    CHECK_EQ(window_title::title("", &s, "prt_fild08", "uaRO HD", Lang::Uk), std::string("uaRO HD"));
    CHECK_EQ(window_title::title("", nullptr, "", "uaRO", Lang::En), std::string("uaRO"));
}

TEST(window_title_another_build_the_name_and_the_map) {
    CHECK_EQ(window_title::title("Lena", nullptr, "prontera", "uaRO HD", Lang::Uk),
             std::string("Lena · prontera - uaRO HD"));
    CHECK_EQ(window_title::title("Lena", nullptr, "", "uaRO HD", Lang::Uk), std::string("Lena - uaRO HD"));
}

TEST(window_title_the_jobs_experience_once_the_base_is_at_its_last) {
    charstatus::Status s = lena();
    s.baseNext = 0;
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::Uk),
             std::string("Lena · 99/70 · 12,0 % · HP 85 % · SP 40 % - uaRO HD"));
    s.jobNext = 0;  // both at their last: no percent
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::Uk),
             std::string("Lena · 99/70 · HP 85 % · SP 40 % - uaRO HD"));
}

TEST(window_title_experience_down_to_a_tenth_never_100) {
    charstatus::Status s = lena();
    s.baseExp = 9999;
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::Uk),
             std::string("Lena · 99/70 · 99,9 % · HP 85 % · SP 40 % - uaRO HD"));
    s.baseExp = 12000;  // over the next for a moment: still 99,9
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::Ru),
             std::string("Lena · 99/70 · 99,9 % · HP 85 % · SP 40 % - uaRO HD"));
    s.baseExp = 2000000000;  // the 2007 client's most: no overflow
    s.baseNext = 2100000000;
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::En),
             std::string("Lena · 99/70 · 95.2 % · HP 85 % · SP 40 % - uaRO HD"));
}

TEST(window_title_hp_a_little_left_is_not_none) {
    charstatus::Status s = lena();
    s.hp = 3;
    s.hpMost = 10000;
    s.sp = 0;
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::Uk),
             std::string("Lena · 99/70 · 45,2 % · HP 1 % · SP 0 % - uaRO HD"));
    s.hp = 0;  // dead
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::Uk),
             std::string("Lena · 99/70 · 45,2 % · HP 0 % · SP 0 % - uaRO HD"));
}

TEST(window_title_weight_from_half) {
    charstatus::Status s = lena();
    s.weight = 499;
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::Uk),
             std::string("Lena · 99/70 · 45,2 % · HP 85 % · SP 40 % - uaRO HD"));
    s.weight = 520;
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::Uk),
             std::string("Lena · 99/70 · 45,2 % · HP 85 % · SP 40 % · вага 52 % - uaRO HD"));
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::Ru),
             std::string("Lena · 99/70 · 45,2 % · HP 85 % · SP 40 % · вес 52 % - uaRO HD"));
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::En),
             std::string("Lena · 99/70 · 45.2 % · HP 85 % · SP 40 % · weight 52 % - uaRO HD"));
}

TEST(window_title_numbers_the_game_has_not_had_are_left_out) {
    const charstatus::Status none;
    CHECK_EQ(window_title::title("Lena", &none, "prontera", "uaRO HD", Lang::Uk),
             std::string("Lena · prontera - uaRO HD"));
    charstatus::Status s = lena();
    s.jobLevel = 0;
    s.hpMost = 0;
    s.spMost = -5;  // not a number the game keeps
    s.baseLevel = 1000;
    CHECK_EQ(window_title::title("Lena", &s, "", "uaRO HD", Lang::Uk), std::string("Lena · 45,2 % - uaRO HD"));
}
