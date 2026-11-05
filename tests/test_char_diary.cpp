// 3.0.11-beta.273 (the owner's features 3 and 4): the characters' diary - its lines read and written back, how long
// ago in words
#include <string>

#include <cstring>
#include <vector>

#include "core/char_diary.hpp"
#include "core/shot_spec.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(char_diary_lines_and_ago) {
    chardiary::Diary d;
    chardiary::Entry e;
    e.lastSeen = 1793900000;
    e.map = "payon";
    e.level = 38;
    e.jobLevel = 27;
    e.job = 3;
    e.played = 7200;
    d["Лучниця"] = e;
    const chardiary::Diary back = chardiary::parse(chardiary::text(d));
    CHECK(back.size() == 1 && back.at("Лучниця").map == "payon" && back.at("Лучниця").played == 7200);
    CHECK(back.at("Лучниця").level == 38 && back.at("Лучниця").job == 3);
    CHECK(chardiary::parse("broken line\nx\t1\n").empty());
    const chardiary::AgoTexts t{"щойно", "{1} хв тому", "{1} год тому", "{1} дн. тому", "{1} міс. тому"};
    const long long now = 1793900000;
    CHECK(chardiary::ago(now, now - 30, t) == "щойно");
    CHECK(chardiary::ago(now, now - 600, t) == "10 хв тому");
    CHECK(chardiary::ago(now, now - 5 * 3600, t) == "5 год тому");
    CHECK(chardiary::ago(now, now - 3 * 86400, t) == "3 дн. тому");
    CHECK(chardiary::ago(now, now - 90 * 86400, t) == "3 міс. тому");
    CHECK(chardiary::ago(now, 0, t).empty());
}

// 3.0.11-beta.273 (the owner's feature 1): a screenshot's target with a part of the picture, its pixels; a .bgra read
// back
TEST(shot_spec_crop_and_raw) {
    const shotspec::Spec plain = shotspec::parse("C:\\shots\\a.png");
    CHECK(!plain.crop && plain.path == "C:\\shots\\a.png");
    const shotspec::Spec s = shotspec::parse(shotspec::withCrop("p.bgra", 0.25f, 0.5f, 0.5f, 0.25f));
    CHECK(s.crop && s.path == "p.bgra");
    int x = 0, y = 0, w = 0, h = 0;
    shotspec::pixels(s, 1920, 1080, x, y, w, h);
    CHECK(x == 480 && y == 540 && w == 960 && h == 270);
    std::string bytes(12, '\0');
    const uint32_t head[3] = {shotspec::kMagic, 2, 1};
    std::memcpy(bytes.data(), head, 12);
    const uint32_t px[2] = {0x00112233u, 0x00445566u};
    bytes.append(reinterpret_cast<const char*>(px), 8);
    std::vector<uint32_t> argb;
    CHECK(shotspec::readRaw(bytes, argb, w, h) && w == 2 && h == 1 && argb[1] == 0xFF445566u);
    CHECK(!shotspec::readRaw("junk", argb, w, h));
}

// 3.0.11-beta.274 (feature 4): a session's gains - level 37 at 80 % to 38 at 20 % is 40 % of a level; its line
TEST(char_diary_session_summary) {
    CHECK_EQ(chardiary::gained(37, 800, 1000, 38, 200, 1000), 40);
    CHECK_EQ(chardiary::gained(10, 0, 100, 12, 50, 200), 225);
    chardiary::Entry e;
    e.sessionSeconds = 4800;
    e.baseGain = 40;
    e.zenyGain = 12500;
    const chardiary::SummaryTexts t{"Остання гра {1}:", "{1} год {2} хв", "{1} хв", "Base +{1} %", "Job +{1} %",
                                    "{1} z"};
    CHECK(chardiary::summary("Ліра", e, t, ',') ==
          "Остання гра Ліра: 1 год 20 хв \xC2\xB7 Base +40 % \xC2\xB7 +12 500 z");
    e.sessionSeconds = 30;
    CHECK(chardiary::summary("Ліра", e, t, ',').empty());  // under a minute: none
    chardiary::Diary d;
    e.sessionSeconds = 600;
    d["Ліра"] = e;
    CHECK(chardiary::parse(chardiary::text(d)).at("Ліра").zenyGain == 12500);
}
