// 3.0.22-beta.6 (the owner's «ок» to idea 10 - the 72/48 judder at 4K 175 %): the game's window made per-monitor DPI
// aware where Windows' scaling leaves a DPI-unaware window short of its monitor (core/dpi_aware)
#include <string>

#include "core/config.hpp"
#include "core/dpi_aware.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(dpi_aware_inexact_scales_only) {
    CHECK(dpiaware::inexact(3840, 2160, 168));   // 175 %: 2194.29 points - the owner's 3839x2159 (R-PSFZ8K)
    CHECK(dpiaware::inexact(1366, 768, 120));    // 125 %: 1092.8
    CHECK(dpiaware::inexact(2560, 1440, 144));   // 150 %: 1706.67
    CHECK(!dpiaware::inexact(1920, 1080, 120));  // 125 %: 1536 x 864 exactly
    CHECK(!dpiaware::inexact(3840, 2160, 144));  // 150 %: 2560 x 1440
    CHECK(!dpiaware::inexact(1920, 1080, 96));   // no scaling
    CHECK(!dpiaware::inexact(1920, 1080, 0));    // not known
}

TEST(dpi_aware_wanted_by_its_mode) {
    CHECK(dpiaware::wanted(DpiAware::Auto, 1, 3840, 2160, 168));
    CHECK(!dpiaware::wanted(DpiAware::Auto, 0, 3840, 2160, 168));   // a window: its size stays as Windows makes it
    CHECK(!dpiaware::wanted(DpiAware::Auto, -1, 3840, 2160, 168));  // the screen's mode not known
    CHECK(!dpiaware::wanted(DpiAware::Auto, 1, 1920, 1080, 120));   // whole points: nothing to mend
    CHECK(dpiaware::wanted(DpiAware::On, 0, 1920, 1080, 96));
    CHECK(!dpiaware::wanted(DpiAware::Off, 1, 3840, 2160, 168));
}

TEST(dpi_aware_its_key) {
    CHECK(parseConfig("").dpiAware == DpiAware::Auto);
    CHECK(parseConfig("[display]\ndpi_aware = 1\n").dpiAware == DpiAware::On);  // the 3.0.11-beta.292 trial's on
    CHECK(parseConfig("[display]\ndpi_aware = 0\n").dpiAware == DpiAware::Off);
    CHECK(parseConfig("[display]\ndpi_aware = auto\n").dpiAware == DpiAware::Auto);
    CHECK(std::string(dpiAwareToken(DpiAware::Auto)) == "auto");
}
