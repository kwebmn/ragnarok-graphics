// 3.0.22-beta.5 (the owner's «ок» to idea 4: 9 of 16 computers laptops, the Radeon R7 M370's 39 frames a second): the
// set «Для ноутбука» - «Авто», «HD економно», the battery saver - and the frames the game measured with it and without
// it, plugged in, on the maps (core/laptop_set)
#include <string>

#include "core/config.hpp"
#include "core/laptop_set.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(laptop_set_keys_are_settings_rows) {
    const auto keys = laptopset::keys();
    CHECK(keys.size() == 6);  // effects.preset, effects.tier, ai.enabled, ai.scale, ai.compress + the battery saver
    bool battery = false;
    for (const auto& [key, token] : keys) {
        const settings::IniSetting* s = settings::findSetting(key);
        CHECK(s != nullptr);
        if (s) CHECK(settings::validToken(*s, token));
        battery = battery || (key == "frames.battery_saver" && token == "1");
    }
    CHECK(battery);
}

TEST(laptop_set_on_only_with_all_its_keys) {
    CHECK(!laptopset::on(parseConfig("")));  // the defaults: HD, no battery saver
    const Config set = parseConfig(
        "[effects]\npreset = auto\ntier = auto\n[ai]\nenabled = 1\nscale = 2\ncompress = on\n"
        "[frames]\nbattery_saver = 1\n");
    CHECK(laptopset::on(set));
    const Config noSaver =
        parseConfig("[effects]\npreset = auto\ntier = auto\n[ai]\nenabled = 1\nscale = 2\ncompress = on\n");
    CHECK(!laptopset::on(noSaver));
}

TEST(laptop_set_meter_keeps_a_rolling_half_hour) {
    laptopset::Meter m;
    for (int i = 0; i < 40 * 60 * 40; ++i) m.frame(1.0 / 40.0, false);  // 40 minutes at 40 frames a second
    CHECK(m.record().other.minutes <= laptopset::kMostMinutes + 0.01);
    CHECK(m.record().other.minutes >= laptopset::kMostMinutes - 0.01);
    CHECK(m.record().other.fps > 39.9 && m.record().other.fps < 40.1);
    for (int i = 0; i < 6 * 60 * 55; ++i) m.frame(1.0 / 55.0, true);  // 6 minutes with the set at 55
    CHECK(m.record().laptop.minutes > 5.9 && m.record().laptop.minutes < 6.1);
    CHECK(m.record().laptop.fps > 54.9 && m.record().laptop.fps < 55.1);
    CHECK(m.record().other.fps > 39.9 && m.record().other.fps < 40.1);  // untouched
}

TEST(laptop_set_records_and_tries_read_back) {
    laptopset::Record r;
    r.other = {39.24, 12.5};
    r.laptop = {52.0, 7.0};
    laptopset::Record back;
    CHECK(laptopset::parse(laptopset::format(r), back));
    CHECK(back.other.fps > 39.1 && back.other.fps < 39.3 && back.other.minutes > 12.4 && back.other.minutes < 12.6);
    CHECK(back.laptop.fps > 51.9 && back.laptop.minutes > 6.9);
    laptopset::Meter m;
    m.set(back);  // the driver goes on from the file
    m.frame(0.02, true);
    CHECK(m.record().laptop.minutes > 7.0);
    laptopset::Try t;
    t.before = 39.2;
    t.undo = {{"ai.scale", "4"}, {"frames.battery_saver", "0"}};
    laptopset::Try tb;
    CHECK(laptopset::parse(laptopset::format(t), tb));
    CHECK(tb.before > 39.1 && tb.before < 39.3 && !tb.told && !tb.declined && tb.undo == t.undo);
    CHECK(!laptopset::parse(std::string("garbage"), back));
}

TEST(laptop_set_plate_offers_then_tells_what_it_gave) {
    using laptopset::Plate;
    laptopset::Record r;
    laptopset::Try t;
    r.other = {39.0, 12.0};
    CHECK(laptopset::plate(false, false, r, t) == Plate::None);  // not a laptop
    CHECK(laptopset::plate(true, false, r, t) == Plate::Offer);
    r.other = {39.0, 3.0};
    CHECK(laptopset::plate(true, false, r, t) == Plate::None);  // too little play yet
    r.other = {58.0, 12.0};
    CHECK(laptopset::plate(true, false, r, t) == Plate::None);  // fast enough
    r.other = {39.0, 12.0};
    t.declined = true;
    CHECK(laptopset::plate(true, false, r, t) == Plate::None);  // «✕»
    t = laptopset::Try{};
    t.before = 39.0;
    r.laptop = {41.0, 2.0};
    CHECK(laptopset::plate(true, true, r, t) == Plate::None);  // tried, not measured enough yet
    r.laptop = {52.0, 6.0};
    CHECK(laptopset::plate(true, true, r, t) == Plate::Better);
    r.laptop = {41.0, 6.0};
    CHECK(laptopset::plate(true, true, r, t) == Plate::NoBetter);
    CHECK(laptopset::plate(true, false, r, t) == Plate::None);  // the set changed since: nothing to tell
    t.told = true;
    CHECK(laptopset::plate(true, true, r, t) == Plate::None);
}
