// The game's key on the settings app's pages (stage 2 spec §6.1, release 0.6.2; docs/research/registry.md).
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/settings/game_settings.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

const RegSetting& setting(const char* key) { return *findRegSetting(key); }

// The common copy holding these "subkey\name" values.
GameRegistry withValues(const std::vector<std::pair<std::string, RegValue>>& values) {
    GameRegistry r;
    r.common.exists = true;
    for (const auto& v : values) {
        const size_t slash = v.first.rfind('\\');
        const std::string subkey = slash == std::string::npos ? "" : v.first.substr(0, slash);
        const std::string name = slash == std::string::npos ? v.first : v.first.substr(slash + 1);
        r.common.keys[subkey][name] = v.second;
    }
    return r;
}

}  // namespace

TEST(game_settings_read_values_by_the_game_s_rules) {
    const GameRegistry r = withValues({{"SPRITEMODE", RegValue::dword(5)},  // 2 and any other value: full size
                                       {"TEXTUREMODE", RegValue::dword(1)},
                                       {"ISLIGHTMAP", RegValue::dword(2)},       // on only when exactly 1
                                       {"DIGITALRATETYPE", RegValue::dword(7)},  // 2 and up: 8000 Hz
                                       {"Option\\bgmIsPaused", RegValue::dword(1)},
                                       {"Option\\streamVolume", RegValue::dword(300)},  // kept raw, played at 127
                                       {"UIRectInfo\\M_MINIMAPARGB", RegValue::dword(0x12345678)},
                                       {"Option\\SKINNAME", RegValue::sz("default")}});
    CHECK(regWordOf(setting("SPRITEMODE"), r) == "2");
    CHECK(regWordOf(setting("TEXTUREMODE"), r) == "1");
    CHECK(regWordOf(setting("ISLIGHTMAP"), r) == "0");
    CHECK(regWordOf(setting("DIGITALRATETYPE"), r) == "2");
    CHECK(regWordOf(setting("Option\\bgmIsPaused"), r) == "0");  // the music is off
    CHECK(regWordOf(setting("Option\\streamVolume"), r) == "127");
    CHECK(regWordOf(setting("UIRectInfo\\M_MINIMAPARGB"), r) == "1");  // anything but opaque is translucent
    CHECK(regWordOf(setting("Option\\SKINNAME"), r) == "default");
}

TEST(game_settings_missing_values_are_the_game_s_own) {
    const GameRegistry r;
    CHECK(regWordOf(setting("SOUNDMODE"), r) == "0");  // no sound at all
    CHECK(regWordOf(setting("NUMSAMPLETYPE"), r) == "2");
    CHECK(regWordOf(setting("FOG"), r) == "1");
    CHECK(regWordOf(setting("Option\\soundVolume"), r) == "100");
    CHECK(regWordOf(setting("Option\\bgmIsPaused"), r) == "1");  // the music is on
    CHECK(regWordOf(setting("Option\\SKINNAME"), r) == "<Basic Skin>");
    CHECK(regWordOf(setting("ShortCutList\\0"), r) == "");
    CHECK(regWordOf(setting("CHECKSAVE"), r) == "0");
    CHECK(std::string(builtInMacro(0)) == "/!");  // the empty macro's text, shown grey
    CHECK(std::string(builtInMacro(9)) == "/...");
}

TEST(game_settings_write_what_the_game_reads) {
    CHECK(regValueOf(setting("Option\\bgmIsPaused"), "1") == RegValue::dword(0));  // music on: not paused
    CHECK(regValueOf(setting("UIRectInfo\\M_MINIMAPARGB"), "0") == RegValue::dword(0xFFFFFFFF));
    CHECK(regValueOf(setting("UIRectInfo\\M_MINIMAPARGB"), "1") == RegValue::dword(0xAAFFFFFF));
    CHECK(regValueOf(setting("SPRITEMODE"), "1") == RegValue::dword(1));
    CHECK(regValueOf(setting("BINKMODE"), "1") == RegValue::dword(0));  // the intro plays
    CHECK(regValueOf(setting("ShortCutList\\3"), "/lv") == RegValue::sz("/lv"));
}

TEST(game_settings_take_only_their_words) {
    CHECK(validRegWord(setting("SPRITEMODE"), "0"));
    CHECK(!validRegWord(setting("SPRITEMODE"), "3"));
    CHECK(!validRegWord(setting("Option\\isEffectOn"), "2"));
    CHECK(validRegWord(setting("Option\\soundVolume"), "127"));
    CHECK(!validRegWord(setting("Option\\soundVolume"), "128"));
    CHECK(validRegWord(setting("ShortCutList\\0"), std::string(49, 'a')));
    CHECK(!validRegWord(setting("ShortCutList\\0"), std::string(50, 'a')));
    std::string cyrillic;
    for (int i = 0; i < 49; ++i) cyrillic += "ж";  // letters, not bytes
    CHECK(validRegWord(setting("ShortCutList\\0"), cyrillic));
    CHECK(!validRegWord(setting("ShortCutList\\0"), "a\nb"));
    CHECK(!validRegWord(setting("Option\\SKINNAME"), ""));
}

TEST(game_settings_the_game_writes_its_own_at_exit) {
    // Spec §4.1: what the game writes itself changes only while it is closed; sound and ISLIGHTMAP any time.
    CHECK(setting("SPRITEMODE").gameWrites);
    CHECK(setting("Option\\isEffectOn").gameWrites);
    CHECK(setting("ShortCutList\\9").gameWrites);
    CHECK(setting("CHECKSAVE").gameWrites);
    CHECK(!setting("ISLIGHTMAP").gameWrites);
    CHECK(!setting("SOUNDMODE").gameWrites);
    CHECK(!setting("SPEAKERTYPE").gameWrites);
}

TEST(game_settings_each_once_on_a_page) {
    std::set<std::string> keys;
    for (const RegSetting& s : regSettings()) {
        CHECK(keys.insert(regKey(s)).second);
        CHECK(findRegSetting(regKey(s)) == &s);
        CHECK(s.page == Page::Quality || s.page == Page::Sound || s.page == Page::Game || s.page == Page::Chat);
        if (s.control == Control::Choice && s.kind == RegKind::Number) CHECK_EQ(s.values.size(), s.choices.size());
        CHECK(validRegWord(s, regWordOf(s, GameRegistry{})));  // the game's own value is one a page can show
    }
    CHECK_EQ(keys.size(), size_t(41));
    CHECK(findRegSetting("Option\\nothing") == nullptr);
}
