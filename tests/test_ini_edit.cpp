#include <fstream>
#include <sstream>

#include "core/config.hpp"
#include "core/settings/ini_edit.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

TEST(ini_edit_replaces_only_the_value) {
    const std::string text =
        "; uaRO Graphics\r\n[quality]\r\nsharpen = 50   ; 0-100\r\nrender_scale = 100\r\n";
    CHECK(applyIniEdits(text, {{"quality", "sharpen", "80"}}) ==
          "; uaRO Graphics\r\n[quality]\r\nsharpen = 80   ; 0-100\r\nrender_scale = 100\r\n");
    CHECK(applyIniEdits("[ROExt]\nWindowWidth=1600\n", {{"ROExt", "WindowWidth", "1920"}}) ==
          "[ROExt]\nWindowWidth=1920\n");
    CHECK(applyIniEdits("[frames]\nvsync = \n", {{"frames", "vsync", "1"}}) == "[frames]\nvsync = 1\n");
}

TEST(ini_edit_matches_sections_and_keys_without_case) {
    // ROExt's ini is read by Windows, which ignores case; the line keeps its own spelling.
    CHECK(applyIniEdits("[ROExt]\nwindowwidth=1600\n", {{"roext", "WindowWidth", "1920"}}) ==
          "[ROExt]\nwindowwidth=1920\n");
    CHECK(applyIniEdits("[ROExt]\nWindowLock=1\n", {{"roext", "WindowWidth", "1920"}}) ==
          "[ROExt]\nWindowLock=1\nWindowWidth = 1920\n");
}

TEST(ini_edit_adds_a_missing_key_to_its_section) {
    const std::string text = "[display]\nmode = auto\n\n[quality]\nsharpen = 50\n";
    CHECK(applyIniEdits(text, {{"display", "monitor", "1"}, {"display", "crisp_text", "0"}}) ==
          "[display]\nmode = auto\nmonitor = 1\ncrisp_text = 0\n\n[quality]\nsharpen = 50\n");
    CHECK(applyIniEdits("[display]\n\n[quality]\n", {{"display", "monitor", "1"}}) ==
          "[display]\nmonitor = 1\n\n[quality]\n");
}

TEST(ini_edit_adds_a_missing_section_at_the_end) {
    CHECK(applyIniEdits("[display]\nmode = auto\n", {{"utility", "language", "ru"}}) ==
          "[display]\nmode = auto\n\n[utility]\nlanguage = ru\n");
    CHECK(applyIniEdits("[display]\nmode = auto", {{"display", "monitor", "1"}}) ==
          "[display]\nmode = auto\nmonitor = 1\n");
    CHECK(applyIniEdits("", {{"utility", "language", "uk"}}) == "[utility]\r\nlanguage = uk\r\n");
}

TEST(ini_edit_handles_sections_repeated_by_install_bat) {
    // install.bat appends "[quality] sharpen" and "[display] crisp_text" to older files: every copy of a key changes,
    // and a new key goes into the last copy of its section.
    const std::string text =
        "[display]\r\nmode = auto\r\n[quality]\r\nrender_scale = 100\r\n\r\n[quality]\r\nsharpen = 50\r\n"
        "\r\n[display]\r\ncrisp_text = 1\r\n";
    const std::string out = applyIniEdits(text, {{"quality", "sharpen", "70"}, {"display", "monitor", "2"}});
    CHECK(out ==
          "[display]\r\nmode = auto\r\n[quality]\r\nrender_scale = 100\r\n\r\n[quality]\r\nsharpen = 70\r\n"
          "\r\n[display]\r\ncrisp_text = 1\r\nmonitor = 2\r\n");
    const Config c = parseConfig(out);
    CHECK_EQ(c.sharpen, 70);
    CHECK_EQ(c.monitor, 2);
}

TEST(ini_edit_keeps_bom_cyrillic_and_crlf) {
    const std::string text = "\xEF\xBB\xBF; Настройки\r\n[utility]\r\nlanguage = en ; язык\r\n";
    CHECK(applyIniEdits(text, {{"utility", "language", "ru"}}) ==
          "\xEF\xBB\xBF; Настройки\r\n[utility]\r\nlanguage = ru ; язык\r\n");
}

TEST(ini_edit_reads_lines_the_way_the_parser_does) {
    // The same key in another section, a commented-out key, a section header with spaces and a comment.
    const std::string text = "[ quality ] ; q\nsharpen = 10\n; sharpen = 99\n[frames]\nsharpen = 0\n";
    CHECK(applyIniEdits(text, {{"quality", "sharpen", "20"}}) ==
          "[ quality ] ; q\nsharpen = 20\n; sharpen = 99\n[frames]\nsharpen = 0\n");
}

TEST(ini_read_gives_the_values_parse_config_uses) {
    const auto v = readIni(
        "\xEF\xBB\xBF[Display]\r\nMode = Window ; note\r\n[quality]\r\nsharpen = 10\r\n[quality]\r\nsharpen = 30\r\n"
        "; render_scale = 200\r\nbroken line\r\n[ROExt]\r\nWindowWidth=1600\r\n");
    CHECK(v.at("display.mode") == "Window");
    CHECK(v.at("quality.sharpen") == "30");  // the last copy, as parseConfig
    CHECK(v.count("quality.render_scale") == 0);
    CHECK(v.at("roext.windowwidth") == "1600");
}

namespace {

std::string shippedIni() {
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/uaro-graphics.ini", std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

TEST(ini_edit_changes_the_shipped_file_in_place) {
    const std::string text = shippedIni();
    CHECK(!text.empty());
    const std::string out = applyIniEdits(text, {{"quality", "sharpen", "90"}, {"display", "crisp_text", "0"}});
    const Config c = parseConfig(out);
    CHECK_EQ(c.sharpen, 90);
    CHECK(!c.crispText);
    CHECK_EQ(out.size(), text.size());  // "50" -> "90", "1" -> "0": the same length, nothing added
}
