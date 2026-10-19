#include <fstream>
#include <sstream>

#include "core/live_config.hpp"
#include "core/settings/auto_setup.hpp"
#include "core/settings/presets.hpp"
#include "core/settings/recommended.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

namespace {

std::string shippedIni() {
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/dist/uaro-graphics.ini", std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

TEST(recommended_takes_auto_setup_over_defaults) {
    Machine m;
    m.windows10 = m.d3d12 = m.d3d11 = true;
    m.d3d11Level = 0xc100;
    m.vendor = 0x10de;
    m.vramMb = 16384;
    m.adapter = "NVIDIA GeForce RTX 5060 Ti";
    m.screenWidth = 2560;
    m.screenHeight = 1440;
    const AutoSetup a = autoSetup(m);
    const Config defaults = parseConfig(shippedIni());
    const std::map<std::string, std::string> tokens = recommendedTokens(defaults, a);
    CHECK(tokens.size() == iniSettings().size());
    for (const IniEdit& e : a.edits)
        if (findSetting(e.section + "." + e.key)) CHECK(tokens.at(e.section + "." + e.key) == e.value);
    bool untouched = false;  // a row the setup leaves keeps the shipped value
    for (const IniSetting& s : iniSettings()) {
        bool edited = false;
        for (const IniEdit& e : a.edits) edited = edited || e.section + "." + e.key == sectionKey(s);
        if (edited) continue;
        untouched = true;
        CHECK(tokens.at(sectionKey(s)) == tokenOf(s, defaults));
    }
    CHECK(untouched);
    CHECK(!a.dlss.empty() && tokens.at("quality.dlss") == a.dlss);
}

TEST(main_quality_round_trip) {
    const std::string ini = shippedIni();
    for (MainQuality q : {MainQuality::Low, MainQuality::Medium, MainQuality::High, MainQuality::SuperLight}) {
        const std::vector<IniEdit> edits = mainQualityEdits(q);
        CHECK(!edits.empty());
        for (const IniEdit& e : edits) CHECK(e.section != "frames" && e.section != "effects");
        CHECK(mainQualityOf(parseConfig(applyIniEdits(ini, edits))) == q);
    }
    // from Super-light back to a preset: super_light goes off
    const std::string light = applyIniEdits(ini, mainQualityEdits(MainQuality::SuperLight));
    CHECK(mainQualityOf(parseConfig(applyIniEdits(light, mainQualityEdits(MainQuality::High)))) == MainQuality::High);
    Config plain;  // none of the presets' values
    plain.renderScale = 130;
    CHECK(!mainQualityOf(plain));
}
