// 3.0.17-beta.18 (the owner: «функционал старого и нового не пересекались», «чини если что старый при запуске
// нового»): uaRO HD's screen lives in its own file, the driver gives it to the game; the shared key keeps the old
// client's setup.exe screen
#include "core/settings/game_screen.hpp"
#include "test.hpp"

using namespace uagfx::settings;

TEST(game_screen_file_round_trip) {
    GameScreen s{true, 2194, 1234};
    const auto back = parseGameScreen(gameScreenText(s));
    CHECK(back && back->fullscreen && back->width == 2194 && back->height == 1234);
    CHECK(!parseGameScreen(""));
    CHECK(!parseGameScreen("fullscreen=1\nwidth=0\nheight=600\n"));     // no size: not a screen
    CHECK(!parseGameScreen("fullscreen=1\nwidth=99999\nheight=600\n"));  // past any monitor
    const auto crlf = parseGameScreen("width=800\r\nheight=600\r\nfullscreen=0\r\n");
    CHECK(crlf && !crlf->fullscreen && crlf->width == 800);
}

TEST(game_screen_the_values_the_game_reads) {
    const GameScreen s{false, 1920, 1080};
    RegValue v;
    CHECK(screenValue(s, "WIDTH", v) && v.asDword() == 1920u);
    CHECK(screenValue(s, "height", v) && v.asDword() == 1080u);  // names without regard to case
    CHECK(screenValue(s, "ISFULLSCREENMODE", v) && v.asDword() == 0u);
    CHECK(screenValue(s, "BITPERPIXEL", v) && v.asDword() == 32u);
    CHECK(screenValue(s, "DEVICENAME", v) && v.isSz() && v.text == "Direct3D HAL");
    CHECK(screenValue(s, "GUIDDEVICE", v) && v.bytes.size() == 16 && v.bytes[0] == 0xE0);
    CHECK(screenValue(s, "GUIDDRIVER", v) && v.bytes == std::vector<uint8_t>(16, 0));
    CHECK(!screenValue(s, "TEXTUREMODE", v));
}

TEST(game_screen_what_the_game_writes_goes_to_the_file) {
    GameScreen s{false, 1920, 1080};
    CHECK(takeScreenWrite(s, "WIDTH", RegValue::dword(2560)) && s.width == 2560);
    CHECK(takeScreenWrite(s, "ISFULLSCREENMODE", RegValue::dword(1)) && s.fullscreen);
    CHECK(takeScreenWrite(s, "BITPERPIXEL", RegValue::dword(16)));  // swallowed: always 32 for the driver
    CHECK(!takeScreenWrite(s, "SPRITEMODE", RegValue::dword(2)));
    CHECK(!takeScreenWrite(s, "HEIGHT", RegValue::sz("x")) && s.height == 1080);  // not a number: the registry's
}

TEST(game_screen_of_the_registry) {
    GameRegistry r;
    r.personal.exists = true;
    r.personal.keys[""]["WIDTH"] = RegValue::dword(2194);
    r.personal.keys[""]["HEIGHT"] = RegValue::dword(1234);
    r.personal.keys[""]["ISFULLSCREENMODE"] = RegValue::dword(1);
    const auto s = screenOf(r);
    CHECK(s && s->fullscreen && s->width == 2194);
    CHECK(!screenOf(GameRegistry{}));
}

namespace {

GameRegistry sharedKey(bool setupInCommon) {
    GameRegistry r;
    r.common.exists = true;
    if (setupInCommon) {
        r.common.keys[""]["WIDTH"] = RegValue::dword(1024);
        r.common.keys[""]["HEIGHT"] = RegValue::dword(768);
        r.common.keys[""]["DEVICENAME"] = RegValue::sz("dgVoodoo HAL");
    }
    r.personal.exists = true;
    r.personal.keys[""]["WIDTH"] = RegValue::dword(2560);
    r.personal.keys[""]["HEIGHT"] = RegValue::dword(1440);
    r.personal.keys[""]["BITPERPIXEL"] = RegValue::dword(32);
    r.personal.keys[""]["DEVICENAME"] = RegValue::sz("Direct3D HAL");
    r.personal.keys[""]["GUIDDRIVER"] = RegValue::binary(std::vector<uint8_t>(16, 0));
    r.personal.keys[""]["TEXTUREMODE"] = RegValue::dword(1);
    r.personal.keys["Option"]["WIDTH"] = RegValue::dword(5);  // not the screen: a subkey's
    return r;
}

}  // namespace

TEST(game_screen_the_shared_key_gets_its_setup_back) {
    const std::vector<RegWrite> w = sharedRepair(sharedKey(true));
    CHECK_EQ(w.size(), size_t(5));  // the screen's five values there; TEXTUREMODE and Option stay
    for (const RegWrite& one : w) {
        CHECK(one.op == RegOp::Delete);
        CHECK(one.subkey.empty());
    }
    CHECK(sharedRepair(sharedKey(false)).empty());  // no setup.exe values under: the old client keeps what it has
}
