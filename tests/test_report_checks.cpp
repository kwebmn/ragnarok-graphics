// 3.0.18-beta.189 (R-VHMGXR «словил глюк с берсрком»: the ground washed, the models not): a report's check shots -
// the same place drawn with one part of ours off each, taken small (core/report_checks, shot_spec's «|/N»)
#include <cstdio>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/report_checks.hpp"
#include "core/shot_spec.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(report_checks_each_variant_held_then_shot) {
    reportchecks::Run run;
    std::string shot;
    CHECK(!run.active());
    CHECK_EQ(run.frame(shot), -1);
    run.start("C:\\Games\\uaRO\\uaro-graphics\\reports\\checks",
              "C:\\Games\\uaRO\\ScreenShot\\uaro-20261112-094332.png");
    CHECK(run.active());
    CHECK_EQ(run.frame(shot), -1);  // the report's own frame: its picture as it is
    CHECK(shot.empty());
    CHECK(run.active());
    std::vector<std::string> shots;
    std::vector<int> drawn;
    for (int i = 0; i < 200 && run.active(); ++i) {
        drawn.push_back(run.frame(shot));
        if (!shot.empty()) shots.push_back(shot);
    }
    const size_t n = reportchecks::variants().size();
    CHECK_EQ(shots.size(), n);
    CHECK_EQ(drawn.size(), n * size_t(reportchecks::kHoldFrames));  // each held, then the next
    CHECK_EQ(drawn.front(), 0);
    CHECK_EQ(drawn[size_t(reportchecks::kHoldFrames)], 1);
    CHECK_EQ(shots.front(), "C:\\Games\\uaRO\\uaro-graphics\\reports\\checks\\uaro-20261112-094332-own-ground.png");
    CHECK_EQ(shots.back(), "C:\\Games\\uaRO\\uaro-graphics\\reports\\checks\\uaro-20261112-094332-pause.png");
    CHECK(!run.active());
    CHECK_EQ(run.frame(shot), -1);
    CHECK(shot.empty());
    run.start("x", "");  // no picture of the report's: no checks
    CHECK(!run.active());
}

TEST(report_checks_fit_the_sites_six_pictures) {
    CHECK(reportchecks::variants().size() + 1 <= size_t(reportchecks::kMostShots));  // with the report's own
    for (const reportchecks::Variant& v : reportchecks::variants()) {
        const std::string name = std::string("check-") + v.name + ".jpg";
        CHECK(name.size() <= 44);  // the site's name: [A-Za-z0-9._-]{1,40}.jpg
        for (char c : std::string(v.name)) CHECK((c >= 'a' && c <= 'z') || c == '-');
    }
    CHECK_EQ(reportchecks::baseOf("D:\\x\\ScreenShot\\uaro-20261112-094332.PNG"), "uaro-20261112-094332");
    CHECK_EQ(reportchecks::baseOf("shot"), "shot");
}

TEST(report_checks_turn_off_their_parts) {
    EffectSettings file = Config{}.effects;
    file.ownGround = file.glow = file.bloom = file.grade = file.lamps = file.torch = file.fireLight = true;
    file.relief = file.ao = true;
    const auto& v = reportchecks::variants();
    const EffectSettings own = reportchecks::effectsOf(v[0], file, 3);
    CHECK(!own.ownGround);
    CHECK(own.relief);  // the rest of the ground as it was
    CHECK(own.glow);
    const EffectSettings ground = reportchecks::effectsOf(v[1], file, 3);
    CHECK(!ground.ownGround);
    CHECK(!ground.relief);
    CHECK(!ground.ao);
    CHECK(ground.lamps);
    const EffectSettings lights = reportchecks::effectsOf(v[2], file, 3);
    CHECK(!lights.lamps);
    CHECK(!lights.glow);
    CHECK(!lights.torch);
    CHECK(!lights.fireLight);
    CHECK(lights.ownGround);
    const EffectSettings screen = reportchecks::effectsOf(v[3], file, 3);
    CHECK(!screen.grade);
    CHECK(!screen.bloom);
    CHECK(screen.ownGround);
    CHECK(v[4].pause);
    const EffectSettings pause = reportchecks::effectsOf(v[4], file, 3);
    CHECK(!pause.ownGround);
    CHECK(!pause.glow);
}

TEST(shot_spec_shrinks_a_whole_picture) {
    const shotspec::Spec s = shotspec::parse(shotspec::withShrink("C:\\a\\b.png", 3));
    CHECK_EQ(s.path, "C:\\a\\b.png");
    CHECK_EQ(s.shrink, 3);
    CHECK(!s.crop);
    CHECK_EQ(shotspec::parse("C:\\a\\b.png|/1").shrink, 1);  // not smaller
    CHECK_EQ(shotspec::parse("C:\\a\\b.png|/40").shrink, 8);
    CHECK_EQ(shotspec::parse("C:\\a\\b.png").shrink, 1);
    // 6 x 4, pitch 7 pixels: three times smaller is 2 x 1, each the mean of its 3 x 3
    std::vector<uint8_t> px(size_t(7) * 4 * 4, 0);
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 6; ++x) {
            uint8_t* p = px.data() + (size_t(y) * 7 + size_t(x)) * 4;
            p[0] = uint8_t(x < 3 ? 30 : 200);  // blue: left 30, right 200
            p[1] = uint8_t(y * 10);            // green by the row (0 10 20 in the first three)
            p[2] = 90;
            p[3] = 255;
        }
    int w = 0, h = 0;
    const std::vector<uint8_t> small = shotspec::shrunk(px.data(), 6, 4, 7 * 4, 3, w, h);
    CHECK_EQ(w, 2);
    CHECK_EQ(h, 1);
    CHECK_EQ(small.size(), size_t(2 * 1 * 4));
    CHECK_EQ(int(small[0]), 30);
    CHECK_EQ(int(small[4]), 200);
    CHECK_EQ(int(small[1]), 10);
    CHECK_EQ(int(small[2]), 90);
    CHECK_EQ(int(small[3]), 255);
}
