#include <fstream>
#include <sstream>

#include "pad/core/config/config.hpp"
#include "pad/core/config/ini_edit.hpp"
#include "pad/core/config/settings.hpp"
#include "test.hpp"

using namespace uapad;

TEST(ini_edit_replaces_only_the_value) {
    const std::string text =
        "; uaRO Pad\r\n[mouse]\r\nfreedom = 1             ; mouse freedom\r\nlock = 0   ; keep it in\r\n";
    CHECK(applyIniEdits(text, {{"mouse", "lock", "1"}}) ==
          "; uaRO Pad\r\n[mouse]\r\nfreedom = 1             ; mouse freedom\r\nlock = 1   ; keep it in\r\n");
    CHECK(applyIniEdits("[mouse]\nlock=0\n", {{"mouse", "lock", "1"}}) == "[mouse]\nlock=1\n");
    CHECK(applyIniEdits("[mouse]\nlock = \n", {{"mouse", "lock", "1"}}) == "[mouse]\nlock = 1\n");
}

TEST(ini_edit_adds_a_missing_key_to_its_section) {
    const std::string text = "[general]\nenabled = 1\n\n[stick]\ndeadzone = 0.25\n";
    CHECK(applyIniEdits(text, {{"general", "language", "uk"}, {"general", "log_level", "info"}}) ==
          "[general]\nenabled = 1\nlanguage = uk\nlog_level = info\n\n[stick]\ndeadzone = 0.25\n");
    CHECK(applyIniEdits("[general]\n\n[stick]\n", {{"general", "language", "ru"}}) ==
          "[general]\nlanguage = ru\n\n[stick]\n");
}

TEST(ini_edit_adds_a_missing_section_at_the_end) {
    CHECK(applyIniEdits("[general]\nenabled = 1\n",
                        {{"window", "borderless", "1"}, {"window", "position", "center"}}) ==
          "[general]\nenabled = 1\n\n[window]\nborderless = 1\nposition = center\n");
    CHECK(applyIniEdits("[general]\nenabled = 1", {{"general", "language", "ru"}}) ==
          "[general]\nenabled = 1\nlanguage = ru\n");
    CHECK(applyIniEdits("", {{"general", "language", "uk"}}) == "[general]\r\nlanguage = uk\r\n");
}

TEST(ini_edit_keeps_bom_cyrillic_and_crlf) {
    const std::string text = "\xEF\xBB\xBF; Настройки\r\n[general]\r\nlanguage = en ; язык меню\r\n";
    CHECK(applyIniEdits(text, {{"general", "language", "ru"}}) ==
          "\xEF\xBB\xBF; Настройки\r\n[general]\r\nlanguage = ru ; язык меню\r\n");
    CHECK(applyIniEdits("\xEF\xBB\xBF[general]\r\nenabled = 1\r\n", {{"general", "enabled", "0"}}) ==
          "\xEF\xBB\xBF[general]\r\nenabled = 0\r\n");
}

TEST(ini_edit_reads_lines_the_way_the_parser_does) {
    // The same key in another section, a commented-out key, a section header with spaces and a comment.
    const std::string text = "[ mouse ] ; the mouse\nlock = 0\n; lock = 1\n[window]\nlock = 0\n[mouse]\nlock = 0\n";
    CHECK(applyIniEdits(text, {{"mouse", "lock", "1"}}) ==
          "[ mouse ] ; the mouse\nlock = 1\n; lock = 1\n[window]\nlock = 0\n[mouse]\nlock = 1\n");
    const Config c = parseConfig(applyIniEdits(text, {{"mouse", "lock", "1"}, {"general", "language", "uk"}}));
    CHECK(c.mouseLock);
    CHECK(c.lang == Lang::Uk);
}

TEST(config_reads_a_file_saved_with_a_bom) {
    CHECK(!parseConfig("\xEF\xBB\xBF[general]\nenabled = 0\n").enabled);
}

namespace {

std::string shippedIni() {
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/pad/uaro-pad.ini", std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

TEST(shipped_ini_holds_the_defaults) {
    const std::string text = shippedIni();
    CHECK(!text.empty());
    const Config c = parseConfig(text);
    const Config d;
    CHECK(c.enabled == d.enabled && c.logLevel == d.logLevel && c.deadzone == d.deadzone);
    CHECK(c.walkRadius == d.walkRadius && c.press == d.press && c.buttons == d.buttons &&
          c.mouseFreedom == d.mouseFreedom);
    CHECK(c.giveUpSeconds == d.giveUpSeconds && c.blacklistSeconds == d.blacklistSeconds);
    CHECK(c.attackIntervalMs == d.attackIntervalMs && c.groundSelf == d.groundSelf && c.traps == d.traps);
    CHECK(c.repeatMs == d.repeatMs && c.position.kind == d.position.kind);
    CHECK(settingsFrom(c) == defaultSettings());
    // Every key of the Settings tab is in the file, so the menu only ever changes a value in place.
    Settings all = defaultSettings();
    for (size_t i = 0; i < kSettingRows; ++i) all = changeSetting(all, SettingRow(i), +1);
    const std::vector<IniEdit> edits = settingsEdits(defaultSettings(), all, 10);
    CHECK_EQ(edits.size(), kSettingRows);
    const std::string edited = applyIniEdits(text, edits);
    CHECK(settingsFrom(parseConfig(edited)) == all);
    size_t lines = 0, changed = 0;
    for (size_t a = 0, b = 0; a < text.size() && b < edited.size(); ++lines) {
        const size_t ea = text.find('\n', a);
        const size_t eb = edited.find('\n', b);
        if (text.substr(a, ea - a) != edited.substr(b, eb - b)) ++changed;
        if (ea == std::string::npos || eb == std::string::npos) break;
        a = ea + 1;
        b = eb + 1;
    }
    CHECK_EQ(changed, kSettingRows);
    CHECK(lines > 30);
}
