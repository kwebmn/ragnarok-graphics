// 3.0.22-beta.2 (the owner: «когда нажимаю на нпс, оно какбуто подлагивает ... какуюто прогрузку сделать еще когда
// грузится»; a Radeon R7 M370 laptop's log: frames of 581 ms round its NPCs' talks, 225 ms of them the game reading its
// archives - an NPC's sprite 60 ms, the talk window's buttons): the files the game read slowly, kept by map, read ahead
// at the next start and map, off the game's thread (core/read_ahead)
#include <string>
#include <vector>

#include "core/read_ahead.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
bool has(const std::vector<std::string>& v, const std::string& s) {
    for (const std::string& x : v)
        if (x == s) return true;
    return false;
}
}  // namespace

TEST(read_ahead_only_slow_reads_are_kept) {
    CHECK(!readahead::slow(0.4));  // from the disk's cache
    CHECK(readahead::slow(readahead::kSlowMs));
    CHECK(readahead::slow(59.8));
}

TEST(read_ahead_a_maps_files_first_then_the_common_ones) {
    readahead::Learned l;
    l.note("prontera", "data\\sprite\\npc\\4_f_03.spr", 1);
    l.note("prontera", "data\\sprite\\npc\\4_f_03.spr", 2);  // twice: first
    l.note("prontera", "data\\wav\\effect\\ef_blessing.wav", 3);
    l.note("prontera", "data\\texture\\ui\\btn_ok.bmp", 4);
    l.note("geffen", "data\\texture\\ui\\btn_ok.bmp", 5);  // slow on two maps: the talk window's, common
    l.note("geffen", "data\\sprite\\npc\\8w_soldier.spr", 6);
    const std::vector<std::string> p = l.forMap("prontera");
    CHECK(!p.empty() && p.front() == "data\\sprite\\npc\\4_f_03.spr");
    CHECK(has(p, "data\\wav\\effect\\ef_blessing.wav") && has(p, "data\\texture\\ui\\btn_ok.bmp"));
    CHECK(!has(p, "data\\sprite\\npc\\8w_soldier.spr"));  // geffen's own
    const std::vector<std::string> a = l.forMap("alberta");  // a map not seen yet: the common ones
    CHECK(a.size() == 1 && a.front() == "data\\texture\\ui\\btn_ok.bmp");
}

TEST(read_ahead_names_alike_whatever_their_case) {
    readahead::Learned l;
    l.note("payon", "data\\Sprite\\NPC\\4_M_01.spr", 1);
    l.note("payon", "data\\sprite\\npc\\4_m_01.spr", 2);
    CHECK_EQ(l.forMap("payon").size(), size_t(1));
}

TEST(read_ahead_kept_in_a_file) {
    readahead::Learned l;
    l.note("prontera", "data\\sprite\\npc\\4_f_03.spr", 7);
    l.note("geffen", "data\\texture\\ui\\btn_ok.bmp", 8);
    l.note("", "data\\texture\\ui\\login_background.bmp", 9);  // the login screens'
    const readahead::Learned back = readahead::Learned::parse(l.text());
    CHECK(back.forMap("prontera") == l.forMap("prontera"));
    CHECK(back.forMap("") == l.forMap(""));
    CHECK_EQ(back.size(), l.size());
    CHECK_EQ(readahead::Learned::parse("not\tlines\nat all").size(), size_t(0));
}

TEST(read_ahead_keeps_so_many_at_most) {
    readahead::Learned l;
    for (size_t i = 0; i < readahead::kPerMap + 50; ++i)
        l.note("moc_fild10", "data\\sprite\\a" + std::to_string(i) + ".spr", i + 1);
    CHECK_EQ(l.forMap("moc_fild10").size(), readahead::kPerMap);
    CHECK(has(l.forMap("moc_fild10"), "data\\sprite\\a" + std::to_string(readahead::kPerMap + 49) + ".spr"));
    for (size_t m = 0; m < readahead::kMaps + 5; ++m) l.note("map" + std::to_string(m), "data\\x.spr", 1000 + m);
    CHECK(l.maps() <= readahead::kMaps);
    CHECK(l.forMap("moc_fild10").size() <= 1);  // the least recent map went (only the common file left)
}
