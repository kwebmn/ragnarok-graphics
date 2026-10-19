#include "core/config.hpp"
#include "core/effect_groups.hpp"
#include "core/light_mode.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(light_modes_take_nothing_when_off) {
    Config c;
    CHECK(light::overridesOf(c, false) == light::Overrides{});
    CHECK(light::overridesOf(c, true) == light::Overrides{});  // the battery saver off: a laptop on its battery too
    c.batterySaver = true;
    CHECK(light::overridesOf(c, false) == light::Overrides{});  // on the mains
    CHECK_EQ(light::fpsLimit(60, light::Overrides{}), 60);
}

TEST(light_super_light_strips_down_to_the_game) {
    Config c;
    c.superLight = true;
    const light::Overrides o = light::overridesOf(c, false);
    CHECK(o.superLight && !o.battery);
    CHECK_EQ(o.effectsLevel, 0);
    CHECK(o.aiHidden && o.noSharpen && o.noAnisotropy && o.noSupersampling);
    CHECK_EQ(o.worldScaleMax, 50);
    CHECK_EQ(o.renderScaleMax, 100);
    // 2.5.22: the world made larger plainly - its own sharpening was the frame's dearest pass (45 taps a pixel)
    CHECK_EQ(int(o.flags), int(light::kFlatWorld | light::kWorld16 | light::kPointMips | light::kPlainUpscale));
    CHECK_EQ(o.fpsMax, 0);  // the frames as the file has them
    EffectSettings e;
    e.preset = EffectPreset::Strong;
    const EffectSettings none = light::effectsOf(effect_groups::effective(e, 3), o);
    CHECK(!none.torch && !none.smoothLight && !none.glow && !none.skyLight);  // the fixes too
}

TEST(light_battery_keeps_the_weak_set_and_the_frames) {
    Config c;
    c.batterySaver = true;
    c.batteryFps = 25;
    const light::Overrides o = light::overridesOf(c, true);
    CHECK(o.battery && !o.superLight && o.pacerSleeps);
    CHECK_EQ(o.effectsLevel, 1);
    CHECK_EQ(o.worldScaleMax, 75);
    CHECK_EQ(o.minimisedFps, 5);
    CHECK_EQ(int(o.flags), int(light::kPlainUpscale));  // 2.5.22: the world at 75 % made larger plainly
    CHECK_EQ(light::fpsLimit(60, o), 25);
    CHECK_EQ(light::fpsLimit(0, o), 25);   // no limit in the file: the battery's
    CHECK_EQ(light::fpsLimit(20, o), 20);  // a lower one stays
    EffectSettings e;
    e.preset = EffectPreset::Strong;
    const EffectSettings strong = effect_groups::effective(e, 3);
    CHECK(strong.torch && strong.glow && strong.skyLight);
    const EffectSettings weak = light::effectsOf(strong, o);
    CHECK(weak.torch && weak.smoothLight);  // the weak set's stay
    CHECK(!weak.glow && !weak.skyLight);    // the medium and strong sets' go
    CHECK(weak.tier <= 1);
    e.preset = EffectPreset::Custom;  // one the player turned off stays off
    e.torch = false;
    CHECK(!light::effectsOf(effect_groups::effective(e, 3), o).torch);
    c.batteryFps = 500;  // out of range: the most it may be
    CHECK_EQ(light::overridesOf(c, true).fpsMax, 60);
}

TEST(light_both_modes_take_the_lighter_of_each) {
    Config c;
    c.superLight = c.batterySaver = true;
    const light::Overrides o = light::overridesOf(c, true);
    CHECK(o.superLight && o.battery);
    CHECK_EQ(o.effectsLevel, 0);
    CHECK_EQ(o.worldScaleMax, 50);
    CHECK_EQ(o.fpsMax, 30);
    CHECK(o.pacerSleeps);
}
