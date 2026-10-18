#include "core/config.hpp"
#include "core/dlss_size.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// DLSS's answers for a 4K screen, as slDLSSGetOptimalSettings gives them: each mode's size and the sizes it takes.
std::vector<dlss::Range> fourK() {
    return {{dlss::Mode::Dlaa, 3840, 2160, 3840, 2160, 3840, 2160},
            {dlss::Mode::Quality, 2560, 1440, 1920, 1080, 3840, 2160},
            {dlss::Mode::Balanced, 2227, 1253, 1920, 1080, 3840, 2160},
            {dlss::Mode::Performance, 1920, 1080, 1920, 1080, 3840, 2160},
            {dlss::Mode::Ultra, 1280, 720, 1280, 720, 3840, 2160}};
}

}  // namespace

TEST(dlss_size_a_share_of_the_screen) {
    // 2.4.2: DLSS's own size - a share of the screen, or the pixels written by hand
    CHECK(dlss::asked(3840, 2160, 75, 0, 0) == (dlss::Size{2880, 1620}));
    CHECK(dlss::asked(3840, 2160, 75, 3000, 1700) == (dlss::Size{3000, 1700}));
    CHECK(dlss::asked(3840, 2160, 75, 3000, 0) == (dlss::Size{3000, 1688}));  // the screen's proportions
    CHECK(dlss::asked(3840, 2160, 75, 0, 1800) == (dlss::Size{3200, 1800}));
    CHECK(dlss::asked(3840, 2160, 250, 0, 0) == (dlss::Size{3840, 2160}));   // never past the screen
    CHECK(dlss::asked(3840, 2160, 0, 0, 0) == (dlss::Size{1, 1}));  // at least a pixel: DLSS's range takes it in
}

TEST(dlss_size_the_mode_whose_range_holds_it) {
    const dlss::Pick p = dlss::pick(fourK(), {2880, 1620});
    CHECK(p.mode == dlss::Mode::Quality && p.size == (dlss::Size{2880, 1620}));
    const dlss::Pick full = dlss::pick(fourK(), {3840, 2160});
    CHECK(full.mode == dlss::Mode::Dlaa && full.size == (dlss::Size{3840, 2160}));
    const dlss::Pick half = dlss::pick(fourK(), {1920, 1080});
    CHECK(half.mode == dlss::Mode::Performance && half.size == (dlss::Size{1920, 1080}));
    const dlss::Pick low = dlss::pick(fourK(), {1000, 562});  // under every mode's range: the nearest it takes
    CHECK(low.mode == dlss::Mode::Ultra && low.size == (dlss::Size{1280, 720}));
    const dlss::Pick none = dlss::pick({}, {1920, 1080});
    CHECK(none.mode == dlss::Mode::None);
}

TEST(dlss_size_from_the_settings_file) {
    const Config d = parseConfig("");
    CHECK(d.dlssScale == 75 && d.dlssWidth == 0 && d.dlssHeight == 0 && d.dlssModel == "auto");
    const Config c = parseConfig(
        "[quality]\ndlss = custom\ndlss_scale = 80\ndlss_width = 3000\ndlss_height = 1700\ndlss_model = l\n");
    CHECK(c.dlss == "custom" && c.dlssScale == 80 && c.dlssWidth == 3000 && c.dlssHeight == 1700 && c.dlssModel == "l");
    CHECK(parseConfig("[quality]\ndlss_scale = 20\n").dlssScale == 75);  // past 33-100: the default stays
    CHECK(parseConfig("[quality]\ndlss_model = z\n").dlssModel == "auto");
}
