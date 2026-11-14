// 3.0.19-beta.40 (the owner's «делаем все по порядку»: in the 3D view the weather fell as flat layers over the
// picture): the 3D view's weather - particles in the world round the eye (weather3d_vs), a field a kind
#include "core/config.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "core/weather3d.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(weather3d_fields_by_kind) {
    const weather3d::Field snow = weather3d::fieldOf(1, 1.0f), rain = weather3d::fieldOf(2, 1.0f),
                           leaves = weather3d::fieldOf(3, 1.0f);
    CHECK(snow.count > 0 && rain.count > snow.count && leaves.count > 0 && leaves.count < snow.count);
    CHECK(rain.fall > snow.fall && snow.fall > leaves.fall && leaves.fall > 0.0f);  // rain fast, leaves slow
    CHECK(rain.streak > 0.0f && snow.streak == 0.0f && leaves.streak == 0.0f);      // rain a streak along its fall
    CHECK(weather3d::fieldOf(5, 1.0f).count > 0 && weather3d::fieldOf(6, 1.0f).count > 0);  // pollen, dust
    CHECK(weather3d::fieldOf(4, 1.0f).count == 0);  // sand: the screen's air as it is
    CHECK(weather3d::fieldOf(0, 1.0f).count == 0 && weather3d::fieldOf(1, 0.0f).count == 0);
    CHECK(weather3d::fieldOf(1, 2.0f).count == 2 * snow.count);  // «Кількість» up to 200 %
    CHECK(weather3d::fieldOf(1, 5.0f).count == 2 * snow.count);
    CHECK(snow.vertices() == snow.count * 6);
    // the box from kUnder under the eye up past the head, wide enough for the fade (half its side) to be far
    for (int kind : {1, 2, 3, 5, 6}) {
        const weather3d::Field f = weather3d::fieldOf(kind, 1.0f);
        CHECK(f.high > weather3d::kUnder + 10.0f && f.side >= 40.0f && f.half > 0.0f);
    }
    CHECK(weather3d::fieldOf(6, 1.0f).blown == 0.0f);  // indoors: no wind
}

// [view3d] weather - the 3D view's weather as particles in the world; on unless the file says 0
TEST(config_view3d_weather_on_by_default_and_live) {
    CHECK(parseConfig("").effects.weather3d);
    CHECK(!parseConfig("[view3d]\nweather = 0\n").effects.weather3d);
    const settings::IniSetting* row = settings::findSetting("view3d.weather");
    CHECK(row != nullptr);
    if (row) CHECK(row->page == settings::Page::Game && row->control == settings::Control::Check);
    CHECK(appliesNow("view3d.weather"));
    CHECK(!(parseConfig("").effects == parseConfig("[view3d]\nweather = 0\n").effects));
}
