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
