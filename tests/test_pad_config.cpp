#include "pad/core/config/config.hpp"
#include "test.hpp"

using namespace uapad;

TEST(config_defaults_when_empty) {
    Config c = parseConfig("");
    CHECK(c.enabled);
    CHECK(c.logLevel == LogLevel::Info);
    CHECK_EQ(c.deadzone, 0.25f);
    CHECK_EQ(c.walkRadius, 0.22f);
    CHECK(c.press == PressMode::Ring);
    CHECK(c.buttons == ButtonChannel::DirectInput);
    CHECK_EQ(c.giveUpSeconds, 4.0f);
    CHECK_EQ(c.blacklistSeconds, 10.0f);
    CHECK_EQ(c.attackIntervalMs, 300);
    CHECK(c.groundSelf == (std::vector<uint16_t>{12, 25, 27, 70}));
    CHECK(c.rumble);
}

TEST(config_reads_all_keys) {
    Config c = parseConfig(
        "[general]\nenabled = 0\nlog_level = debug ; comment\n"
        "[stick]\ndeadzone=0.3\nwalk_radius = 0.3\npress = center\n"
        "[mouse]\nbuttons = messages\n"
        "[target]\ngive_up_seconds = 6\nblacklist_seconds = 20\nattack_interval_ms = 500\n"
        "[skills]\nground_self = 12, 25\n"
        "[rumble]\nenabled = off\n");
    CHECK(!c.enabled);
    CHECK(c.logLevel == LogLevel::Debug);
    CHECK_EQ(c.deadzone, 0.3f);
    CHECK_EQ(c.walkRadius, 0.3f);
    CHECK(c.press == PressMode::Center);
    CHECK(c.buttons == ButtonChannel::Messages);
    CHECK_EQ(c.giveUpSeconds, 6.0f);
    CHECK_EQ(c.blacklistSeconds, 20.0f);
    CHECK_EQ(c.attackIntervalMs, 500);
    CHECK(c.groundSelf == (std::vector<uint16_t>{12, 25}));
    CHECK(!c.rumble);
}

TEST(config_bad_values_keep_defaults) {
    Config c = parseConfig(
        "\xEF\xBB\xBF; file saved with a UTF-8 BOM\n"
        "[stick]\ndeadzone = abc\nwalk_radius = 5\npress = sideways\n"
        "[target]\nattack_interval_ms = 10\n"
        "[skills]\nground_self = 12,x\n"
        "[nonsense]\nfoo = bar\nnot a line\n");
    CHECK_EQ(c.deadzone, 0.25f);
    CHECK_EQ(c.walkRadius, 0.22f);
    CHECK(c.press == PressMode::Ring);
    CHECK_EQ(c.attackIntervalMs, 300);
    CHECK_EQ(c.groundSelf.size(), 4u);
}

TEST(config_empty_ground_self_list_is_allowed) {
    Config c = parseConfig("[skills]\nground_self =\n");
    CHECK(c.groundSelf.empty());
}

TEST(config_mouse_freedom) {
    CHECK(parseConfig("").mouseFreedom);  // on by default: the uaRO client hides the cursor without it
    CHECK(!parseConfig("[mouse]\nfreedom = 0\n").mouseFreedom);
    CHECK(parseConfig("[mouse]\nfreedom = 1\n").mouseFreedom);
}

TEST(config_v2_keys) {
    Config d = parseConfig("");
    CHECK_EQ(d.repeatMs, 250);
    CHECK(d.traps == (std::vector<uint16_t>{115, 116, 117, 118, 119, 120, 121, 122, 123, 125}));
    CHECK(d.mark == MarkStyle::Poison);
    Config c = parseConfig("[skills]\nrepeat_ms = 400\ntraps = 117, 123\n[aim]\nmark = 0\n");
    CHECK_EQ(c.repeatMs, 400);
    CHECK(c.traps == (std::vector<uint16_t>{117, 123}));
    CHECK(c.mark == MarkStyle::Off);
    CHECK_EQ(parseConfig("[skills]\nrepeat_ms = 0\n").repeatMs, 0);    // no repeat
    CHECK_EQ(parseConfig("[skills]\nrepeat_ms = 30\n").repeatMs, 150);  // a slot fires at most every 150 ms anyway
    CHECK_EQ(parseConfig("[skills]\nrepeat_ms = 9000\n").repeatMs, 250);
    CHECK_EQ(parseConfig("[skills]\nrepeat_ms = -5\n").repeatMs, 250);
}

TEST(config_04_defaults) {
    const Config c = parseConfig("");
    CHECK(c.lang == Lang::En);
    CHECK(!c.mouseLock);
    CHECK(!c.focusClick);  // the click that activates the window stays out of the game
    CHECK(c.mouseKeys == (std::array<int, 3>{0, 0, 0}));
    CHECK(!c.borderless);
    CHECK(c.position.kind == WindowPlacement::Kind::Center);
    CHECK_EQ(c.backgroundSleepMs, 10);
}

TEST(config_04_keys) {
    const Config c = parseConfig(
        "[general]\nlanguage = uk\n"
        "[mouse]\nlock = 1\nfocus_click = 1\nmiddle = F3\nbutton4 = f1\nbutton5 = none\n"
        "[window]\nborderless = 1\nposition = -1920, 40\nbackground_sleep_ms = 25\n");
    CHECK(c.lang == Lang::Uk);
    CHECK(c.mouseLock);
    CHECK(c.focusClick);
    CHECK(c.mouseKeys == (std::array<int, 3>{3, 1, 0}));
    CHECK(c.borderless);
    CHECK(c.position.kind == WindowPlacement::Kind::At && c.position.x == -1920 && c.position.y == 40);
    CHECK_EQ(c.backgroundSleepMs, 25);
    CHECK(parseConfig("[window]\nposition = keep\n").position.kind == WindowPlacement::Kind::Keep);
    CHECK(parseConfig("[window]\nbackground_sleep_ms = 0\n").backgroundSleepMs == 0);
}

TEST(config_04_bad_values_keep_defaults) {
    const Config c = parseConfig(
        "[general]\nlanguage = de\n"
        "[mouse]\nmiddle = F10\nbutton4 = G1\nbutton5 = F0\n"
        "[window]\nposition = 10\nbackground_sleep_ms = 101\n");
    CHECK(c.lang == Lang::En);
    CHECK(c.mouseKeys == (std::array<int, 3>{0, 0, 0}));
    CHECK(c.position.kind == WindowPlacement::Kind::Center);
    CHECK_EQ(c.backgroundSleepMs, 10);
    CHECK(parseConfig("[window]\nposition = a,b\n").position.kind == WindowPlacement::Kind::Center);
}

TEST(config_function_key_tokens) {
    CHECK(fKeyToken(0) == "none");
    CHECK(fKeyToken(9) == "F9");
    CHECK(parseFKey("F5") && *parseFKey("F5") == 5);
    CHECK(parseFKey(" none ") && *parseFKey(" none ") == 0);
    CHECK(!parseFKey("F10"));
}

TEST(config_target_mark_styles) {
    CHECK(parseConfig("[aim]\nmark = 1\n").mark == MarkStyle::Poison);  // 0.3 files say 1 / 0: on is the default
    CHECK(parseConfig("[aim]\nmark = gasp\n").mark == MarkStyle::Gasp);
    CHECK(parseConfig("[aim]\nmark = sight\n").mark == MarkStyle::Sight);
    CHECK(parseConfig("[aim]\nmark = Ruwach\n").mark == MarkStyle::Ruwach);
    CHECK(parseConfig("[aim]\nmark = poison\n").mark == MarkStyle::Poison);
    CHECK(parseConfig("[aim]\nmark = dpoison\n").mark == MarkStyle::DeadlyPoison);
    CHECK(parseConfig("[aim]\nmark = curse\n").mark == MarkStyle::Curse);
    CHECK(parseConfig("[aim]\nmark = stun\n").mark == MarkStyle::Stun);
    CHECK(parseConfig("[aim]\nmark = sleep\n").mark == MarkStyle::Sleep);
    CHECK(parseConfig("[aim]\nmark = silence\n").mark == MarkStyle::Silence);
    CHECK(parseConfig("[aim]\nmark = off\n").mark == MarkStyle::Off);
    CHECK(parseConfig("[aim]\nmark = 0\n").mark == MarkStyle::Off);
    CHECK(parseConfig("[aim]\nmark = sparkles\n").mark == MarkStyle::Poison);  // unknown: the default
    for (int i = 0; i < kMarkStyles; ++i)  // every style reads back from its own token
        CHECK(parseMarkStyle(markStyleToken(MarkStyle(i))) &&
              *parseMarkStyle(markStyleToken(MarkStyle(i))) == MarkStyle(i));
}
