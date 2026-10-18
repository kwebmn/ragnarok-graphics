#include "core/effect_groups.hpp"
#include "core/settings/log_digest.hpp"
#include "core/settings/presets.hpp"
#include "core/settings/report.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

TEST(presets_set_the_spec_values) {
    const Config max = parseConfig("[display]\nhigh_resolution = 0\n" +
                                   applyIniEdits("", presetEdits(Preset::Maximum)));
    CHECK(max.highResolution);
    CHECK_EQ(max.renderScale, 200);
    CHECK_EQ(max.sharpen, 50);
    CHECK(max.uiFilter == UiFilter::Sharp);
    CHECK(max.crispText);
    const Config plain = parseConfig(applyIniEdits("", presetEdits(Preset::Plain)));
    CHECK(!plain.highResolution);
    CHECK_EQ(plain.sharpen, 0);
    CHECK(!plain.crispText);
    for (int i = 0; i < kPresetCount; ++i)
        CHECK(presetOf(parseConfig(applyIniEdits("", presetEdits(Preset(i))))) == Preset(i));
}

TEST(presets_the_defaults_are_balance) {
    CHECK(presetOf(Config{}) == Preset::Balance);
    CHECK(!presetOf(parseConfig("[quality]\nsharpen = 70\n")).has_value());
}

TEST(log_digest_reads_the_main_facts) {
    const std::string log =
        "19:20:01.001 INFO  uaRO Graphics 0.6.0 in C:\\Games\\uaRO\\uaRO.exe\r\n"
        "19:20:01.010 INFO  renderer: uaro (backend = uaro, Windows 10, Direct3D 12)\r\n"
        "19:20:02.000 ERROR cannot load C:\\x\\DDraw.dll (error 126)\r\n"
        "19:20:31.000 INFO  3600 frames in 30.0 s (120.0 fps), 0 reads of the frame from the GPU\r\n"
        "19:21:01.000 INFO  3881 frames in 30.0 s (129.4 fps), 1 reads of the frame from the GPU; crisp text: 74 twins\r\n";
    const LogDigest d = digestLog(log, 2);
    CHECK(d.found);
    CHECK(d.version == "0.6.0");
    CHECK(d.renderer == "uaro (backend = uaro, Windows 10, Direct3D 12)");
    CHECK(d.fps == "129.4 fps");
    CHECK_EQ(d.errorCount, size_t(1));
    CHECK(d.errors[0] == "cannot load C:\\x\\DDraw.dll (error 126)");
    CHECK_EQ(d.tail.size(), size_t(2));
    CHECK(d.tail[1].rfind("19:21:01.000", 0) == 0);
    CHECK(!digestLog("").found);
}

TEST(report_video_memory_at_the_32_bit_cap_is_a_floor) {
    // DXGI tells a 32-bit program no more than 3072 MB (the RTX 5060 Ti of 0.1.1's check showed 3072 MB).
    CHECK(vramText(2048) == "2048 MB");
    CHECK(vramText(3071) == "3071 MB");
    CHECK(vramText(3072) == "at least 3072 MB (a 32-bit program sees no more)");
    CHECK(vramText(16303) == "16303 MB");  // 0.51.1: the driver's number from the registry
}

TEST(report_video_memory_past_the_cap_is_the_display_drivers) {
    // 2.2.3: the driver too - DXGI's 3072 MB in the game's 32-bit process made every newer card a weak one (the card's
    // level 1: the automatic set the weak one, while the settings app said strong); the display driver's own number
    // (the registry's) where DXGI's is the cap, and never less than DXGI's
    CHECK_EQ(realVramMb(3072, 16311), 16311ull);
    CHECK_EQ(realVramMb(3072, 0), 3072ull);      // no number in the registry: the floor
    CHECK_EQ(realVramMb(3072, 2048), 3072ull);   // a smaller one is not the card's
    CHECK_EQ(realVramMb(2048, 16311), 2048ull);  // under the cap DXGI's is the card's
    CHECK_EQ(effect_groups::cardLevel(true, realVramMb(3072, 16311)), 3);
}

TEST(report_hides_user_names_and_lists_the_settings) {
    ReportFacts f;
    f.utilityVersion = "0.6.0";
    f.windows = "10.0 build 26200";
    f.gpu = "NVIDIA GeForce RTX 5060 Ti, 16311 MB, Direct3D 12: OK";
    f.monitors = {"3840x2160, 120 Hz, 175 %, HDR no, main"};
    f.iniText = "; comment\r\n[quality]\r\nsharpen = 80 ; note\r\n";
    f.screen = "window 3360x1890, interface 175 %, game 1920x1080";
    f.log = digestLog("19:20:01.001 INFO  uaRO Graphics 0.6.0 in C:\\Users\\Ivan Petrov\\Games\\uaRO\\uaRO.exe\r\n");
    const std::string r = makeReport(f);
    CHECK(r.find("Ivan") == std::string::npos);
    CHECK(r.find("C:\\Users\\...\\Games\\uaRO\\uaRO.exe") != std::string::npos);
    CHECK(r.find("  quality.sharpen = 80\r\n") != std::string::npos);
    CHECK(r.find("comment") == std::string::npos);
    CHECK(r.find("Installed driver: unknown") != std::string::npos);
    CHECK(maskUserPaths("C:/Users/anna/x and D:\\users\\b\\y") == "C:/Users/.../x and D:\\users\\...\\y");
}

TEST(log_digest_knows_the_last_map) {
    // 2.2.5: the Texture pack page's one map - the last the game showed, as a start
    const LogDigest d = digestLog(
        "09:18:12.882 INFO  map: cmd_fild02\r\n"
        "09:18:12.967 INFO  render64: region: Comodo (cmd_fild02)\r\n"
        "09:30:01.000 INFO  map: alde_dun03\r\n"
        "09:30:02.000 INFO  render64: map objects: alde_dun03 from data.grf\r\n");
    CHECK(d.map == "alde_dun03");
    CHECK(digestLog("09:30:01.000 INFO  renderer: uaro\r\n").map.empty());
}

TEST(log_digest_reads_the_crisp_text_lines_of_the_last_start) {
    const std::string log =
        "19:20:01.001 INFO  uaRO Graphics 0.6.0 in C:\\Games\\uaRO\\uaRO.exe\r\n"
        "19:20:01.050 INFO  crisp text: on\r\n"
        "19:20:02.000 INFO  crisp text: k 1.750, 12 windows redrawn\r\n"
        "19:30:01.001 INFO  uaRO Graphics 0.6.1 in C:\\Games\\uaRO\\uaRO.exe\r\n"
        "19:30:01.050 INFO  crisp text: on\r\n"
        "19:30:02.000 INFO  crisp text: k 2.625, 74 windows redrawn\r\n"
        "19:30:03.000 INFO  crisp text: the game's font of height -15 drawn at -37 (k 2.625: sample 812 px)\r\n";
    const LogDigest d = digestLog(log);
    CHECK(d.crisp == "on");
    CHECK(d.crispScale == "k 2.625, 74 windows redrawn");
    const LogDigest off = digestLog(
        "19:20:01.001 INFO  uaRO Graphics 0.6.0 in C:\\Games\\uaRO\\uaRO.exe\r\n"
        "19:20:01.002 INFO  crisp text: k 1.750, 12 windows redrawn\r\n"
        "19:40:01.001 INFO  uaRO Graphics 0.6.1 in C:\\Games\\uaRO\\uaRO.exe\r\n"
        "19:40:01.050 INFO  crisp text: off (the game is not at 0x400000)\r\n");
    CHECK(off.crisp == "off (the game is not at 0x400000)");
    CHECK(off.crispScale.empty());  // the last start's own lines only
}

TEST(presets_set_the_game_s_own_quality) {
    // Spec §8 from 0.6.2: sprites and textures high; 0.87.1: Speed too (it halved the textures); mipmaps never.
    using Words = std::vector<std::pair<std::string, std::string>>;
    CHECK(presetRegWords(Preset::Maximum) == (Words{{"SPRITEMODE", "2"}, {"TEXTUREMODE", "2"}}));
    CHECK(presetRegWords(Preset::Balance) == (Words{{"SPRITEMODE", "2"}, {"TEXTUREMODE", "2"}}));
    CHECK(presetRegWords(Preset::Speed) == (Words{{"SPRITEMODE", "2"}, {"TEXTUREMODE", "2"}}));
    CHECK(presetRegWords(Preset::Plain).empty());
    const Config balance = parseConfig(applyIniEdits("", presetEdits(Preset::Balance)));
    CHECK(presetOf(balance, {{"SPRITEMODE", "2"}, {"TEXTUREMODE", "2"}}) == Preset::Balance);
    CHECK(!presetOf(balance, {{"SPRITEMODE", "2"}, {"TEXTUREMODE", "1"}}).has_value());
    CHECK(presetOf(balance) == Preset::Balance);  // without the game's words: uaro-graphics.ini alone
}
