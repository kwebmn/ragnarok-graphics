#include <fstream>
#include <sstream>
#include <string>

#include "core/ini_upgrade.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

size_t count(const std::string& text, const std::string& what) {
    size_t n = 0;
    for (size_t at = text.find(what); at != std::string::npos; at = text.find(what, at + 1)) ++n;
    return n;
}

std::string sourceFile(const char* relative) {
    std::ifstream f(std::string(UAGFX_SOURCE_DIR) + "/" + relative, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}

std::string smoothing(const std::string& text) {  // the msaa block's three values
    const std::string up = upgradeIni(text);
    const auto value = [&](const std::string& key) {
        const size_t at = up.find("\r\n" + key + " = ");
        return at == std::string::npos ? std::string("?") : up.substr(at + key.size() + 5, up.find('\r', at + 2) -
                                                                                              (at + key.size() + 5));
    };
    return value("msaa") + "/" + value("alpha_to_coverage") + "/" + value("anisotropy");
}

}  // namespace

// 2.10: install.bat's additions to an older uaro-graphics.ini, now the installer's (core/ini_upgrade).
TEST(ini_upgrade_gives_an_old_file_every_block_once) {
    const std::string old = "[display]\r\nmode = window\r\n";
    const std::string up = upgradeIni(old);
    CHECK(up.rfind(old, 0) == 0);  // its own lines first, unchanged
    for (const char* line : {"sharpen = 50", "crisp_text = 1", "msaa = 4", "glow = 0", "water = 0", "ao = 0",
                             "grade = 0", "dof = 0", "smooth_light = 1", "torch = 1", "torch_flame = 1", "smoke = 1",
                             "lamps = 1", "fire_light = 1", "skill_colours = 1", "aura = 1", "skills_soft = 1",
                             "fireflies = 1", "map_glows = 1", "rays = 1", "lava = 1", "portals = 1",
                             "actor_shadows = 1", "heat = 1", "weather = 1", "weather_amount = 100"})
        CHECK_EQ(count(up, std::string("\r\n") + line + "\r\n"), size_t(1));
    CHECK_EQ(count(up, "clouds"), size_t(0));  // the removed effects' blocks are not added
    CHECK_EQ(count(up, "haze"), size_t(0));
    CHECK(up.find("; 0-200 %: how much of it") != std::string::npos);  // install.bat's %% is one %
}

TEST(ini_upgrade_twice_is_once) {
    const std::string once = upgradeIni("[display]\r\nmode = window\r\n");
    CHECK(upgradeIni(once) == once);
}

TEST(ini_upgrade_leaves_the_package_template) {
    const std::string t = sourceFile("dist/uaro-graphics.ini");
    CHECK(!t.empty());
    CHECK(upgradeIni(t) == t);
}

TEST(ini_upgrade_keeps_a_light_preset_light) {
    CHECK_EQ(smoothing("[quality]\r\nsharpen = 50\r\n"), std::string("4/1/16"));
    CHECK_EQ(smoothing("[quality]\r\nsharpen = 0\r\n[display]\r\ncrisp_text = 0\r\n"), std::string("0/1/4"));
    CHECK_EQ(smoothing("[quality]\r\nSHARPEN = 0 ; off\r\n[display]\r\ncrisp_text = 0\r\nhigh_resolution = 0\r\n"),
             std::string("0/0/0"));
    CHECK_EQ(smoothing("[quality]\r\nsharpen = 05\r\n[display]\r\ncrisp_text = 0\r\n"), std::string("4/1/16"));
}

TEST(ini_upgrade_moves_0_17_textures_to_16) {
    const std::string old = "[ai]\r\n; the textures it takes, by their sides in pixels: from min_size to max_size, "
                            "16-1024\r\nmin_size = 64\r\nmax_size = 1024\r\n";
    const std::string up = upgradeIni(old);
    CHECK(up.find("\r\n; the textures of the world it takes, by their sides in pixels (16 since 0.17)\r\nmin_size = "
                  "16\r\nmax_size = 1024\r\n") != std::string::npos);
    CHECK(up.find("min_size = 64") == std::string::npos);
    CHECK(upgradeIni("[ai]\r\nmin_size = 64\r\n").find("min_size = 64") != std::string::npos);  // without the note
}
