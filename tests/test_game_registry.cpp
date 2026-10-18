#include "core/settings/game_registry.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

// The user's PC (2026-10-13): the screen only in the common copy, the game's own saves in the personal one.
GameRegistry usersPc() {
    GameRegistry r;
    r.common.exists = true;
    r.common.keys[""]["WIDTH"] = RegValue::dword(1920);
    r.common.keys[""]["HEIGHT"] = RegValue::dword(1080);
    r.common.keys[""]["ISFULLSCREENMODE"] = RegValue::dword(0);
    r.common.keys[""]["TEXTUREMODE"] = RegValue::dword(2);
    r.common.keys["Option"]["soundVolume"] = RegValue::dword(100);
    r.personal.exists = true;
    r.personal.keys[""]["TEXTUREMODE"] = RegValue::dword(1);
    r.personal.keys[""]["SHOWTIPSATSTARTUP"] = RegValue::dword(0);
    return r;
}

}  // namespace

TEST(registry_the_personal_copy_wins_value_by_value) {
    const GameRegistry r = usersPc();
    CHECK(effectiveValue(r, "", "TEXTUREMODE")->asDword() == 1u);
    CHECK(effectiveValue(r, "", "WIDTH")->asDword() == 1920u);
    CHECK(effectiveValue(r, "", "SHOWTIPSATSTARTUP")->asDword() == 0u);
    CHECK(effectiveValue(r, "option", "SOUNDVOLUME")->asDword() == 100u);  // names ignore case, as in Windows
    CHECK(!effectiveValue(r, "", "SOUNDMODE"));
    CHECK(gameKeyExists(r));
    CHECK(!gameKeyExists(GameRegistry{}));
}

TEST(registry_writes_go_to_the_personal_copy) {
    const GameRegistry r = usersPc();
    const auto w = planWrite(r, "", "WIDTH", RegValue::dword(1680));
    CHECK(w.has_value());
    CHECK(w->op == RegOp::Set);
    CHECK(w->value.asDword() == 1680u);
    // 2.4.2: what the game sees from setup.exe's copy alone is written into the personal one - setup.exe out of it.
    CHECK(planWrite(r, "", "WIDTH", RegValue::dword(1920))->op == RegOp::Set);
    CHECK(!planWrite(r, "", "TEXTUREMODE", RegValue::dword(1)));  // the personal copy holds it already
}

TEST(registry_a_value_equal_to_setup_exes_stays_in_the_personal_copy) {
    // 2.4.2: until now it dropped the personal value, and setup.exe was in charge of it again
    const GameRegistry r = usersPc();
    const auto w = planWrite(r, "", "TEXTUREMODE", RegValue::dword(2));
    CHECK(w.has_value() && w->op == RegOp::Set && w->value.asDword() == 2u);
    CHECK(planWrite(r, "", "TEXTUREMODE", RegValue::sz("2"))->op == RegOp::Set);
}

TEST(registry_takes_what_only_setup_exes_copy_holds) {
    // 2.4.2: every value the game reads from the common copy alone (setup.exe's) goes into the personal one - setup.exe
    // changing it later changes nothing the game sees
    GameRegistry r = usersPc();
    const std::vector<RegWrite> w = pinWrites(r);
    CHECK_EQ(w.size(), size_t(4));  // WIDTH, HEIGHT, ISFULLSCREENMODE, Option's soundVolume - TEXTUREMODE the player's
    applyWrites(r.personal, w);
    CHECK(r.personal.find("", "WIDTH")->asDword() == 1920u);
    CHECK(r.personal.find("Option", "soundVolume")->asDword() == 100u);
    CHECK(r.personal.find("", "TEXTUREMODE")->asDword() == 1u);
    CHECK(pinWrites(r).empty());
    r.common.keys[""]["WIDTH"] = RegValue::dword(800);  // setup.exe run afterwards
    CHECK(effectiveValue(r, "", "WIDTH")->asDword() == 1920u);
}

TEST(registry_applying_writes_updates_the_model) {
    GameRegistry r = usersPc();
    applyWrites(r.personal, {*planWrite(r, "", "WIDTH", RegValue::dword(1680)),
                             *planWrite(r, "", "TEXTUREMODE", RegValue::dword(2))});
    CHECK(effectiveValue(r, "", "WIDTH")->asDword() == 1680u);
    CHECK(effectiveValue(r, "", "TEXTUREMODE")->asDword() == 2u);
    CHECK(r.personal.find("", "TEXTUREMODE")->asDword() == 2u);  // 2.4.2: the player's, though setup.exe's is 2 too
}

TEST(registry_first_run_has_all_the_game_needs) {
    // registry.md §7: the key, WIDTH and HEIGHT not 0, the HAL device, 16 or 32 bits, a sound mode; numbers as DWORDs.
    GameRegistry r;
    applyWrites(r.personal, firstRunWrites(1920, 1080, false));
    CHECK(r.personal.exists);
    CHECK(effectiveValue(r, "", "WIDTH")->asDword() == 1920u);
    CHECK(effectiveValue(r, "", "HEIGHT")->asDword() == 1080u);
    CHECK(effectiveValue(r, "", "ISFULLSCREENMODE")->asDword() == 0u);
    CHECK(effectiveValue(r, "", "BITPERPIXEL")->asDword() == 32u);
    CHECK(effectiveValue(r, "", "SOUNDMODE")->asDword() == 1u);
    const auto guid = effectiveValue(r, "", "GUIDDEVICE");
    CHECK(guid->type == uint32_t(RegType::Binary));
    CHECK(guid->bytes == halDeviceGuid());
    CHECK_EQ(guid->bytes.size(), size_t(16));
    CHECK(guid->bytes[0] == 0xE0 && guid->bytes[3] == 0x84 && guid->bytes[15] == 0x6E);
    for (const RegWrite& w : firstRunWrites(800, 600, true))
        CHECK(w.value.isSz() || w.value.type == uint32_t(RegType::Binary) || w.value.asDword().has_value());
    CHECK(effectiveValue(r, "", "TRILINEARFILTER")->asDword() == 0u);
}

TEST(game_registry_the_effective_copy_is_what_the_game_reads) {
    GameRegistry r;
    r.common.exists = true;
    r.common.keys[""]["WIDTH"] = RegValue::dword(1920);
    r.common.keys[""]["SOUNDMODE"] = RegValue::dword(1);
    r.personal.exists = true;
    r.personal.keys[""]["WIDTH"] = RegValue::dword(1680);
    r.personal.keys["Option"]["isEffectOn"] = RegValue::dword(0);
    const RegCopy c = effectiveCopy(r);
    CHECK(c.exists);
    CHECK(c.find("", "WIDTH") && *c.find("", "WIDTH")->asDword() == 1680u);  // the personal one wins
    CHECK(c.find("", "SOUNDMODE") && *c.find("", "SOUNDMODE")->asDword() == 1u);
    CHECK(c.find("Option", "isEffectOn") && *c.find("Option", "isEffectOn")->asDword() == 0u);
    CHECK(!effectiveCopy(GameRegistry{}).exists);
}

TEST(game_registry_a_window_layout_reset_puts_every_window_back) {
    // Spec §8: every window's .X becomes -1 - it does not fit, so the game puts the window in its place when it
    // opens - and the chat goes to the bottom left; the rest stays.
    GameRegistry r;
    r.personal.exists = true;
    r.personal.keys["UIRectInfo"]["ITEMWNDINFO.X"] = RegValue::dword(1700);
    r.personal.keys["UIRectInfo"]["ITEMWNDINFO.Y"] = RegValue::dword(50);
    r.personal.keys["UIRectInfo"]["M_CHATWNDY"] = RegValue::dword(900);
    r.common.exists = true;
    r.common.keys["UIRectInfo"]["SKILLLISTWNDINFO.X"] = RegValue::dword(300);
    GameRegistry after = r;
    applyWrites(after.personal, windowLayoutReset(r));
    CHECK(effectiveValue(after, "UIRectInfo", "ITEMWNDINFO.X") == RegValue::dword(0xFFFFFFFFu));
    CHECK(effectiveValue(after, "UIRectInfo", "SKILLLISTWNDINFO.X") == RegValue::dword(0xFFFFFFFFu));
    CHECK(effectiveValue(after, "UIRectInfo", "M_CHATWNDY") == RegValue::dword(0xFFFFFFFFu));
    CHECK(effectiveValue(after, "UIRectInfo", "ITEMWNDINFO.Y") == RegValue::dword(50));
    CHECK(windowLayoutReset(GameRegistry{}).empty());  // no layout yet: the game makes its own
}
