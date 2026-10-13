#include "core/settings/screen.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

namespace {

// The user's monitor: 4K at 175 %, the task bar 72 real pixels high.
MonitorInfo fourK() {
    MonitorInfo m;
    m.primary = true;
    m.pixels = {3840, 2160};
    m.work = {3840, 2088};
    m.scale = 175;
    m.refresh = 120;
    m.modes = {{3840, 2160}, {2560, 1440}, {1920, 1080}};
    return m;
}

MonitorInfo fullHd() {
    MonitorInfo m;
    m.pixels = {1920, 1080};
    m.work = {1920, 1040};
    m.scale = 100;
    return m;
}

ScreenChoice window(int w, int h, int ui, int monitor = 0) {
    ScreenChoice c;
    c.show = Show::Window;
    c.window = {w, h};
    c.ui = ui;
    c.monitor = monitor;
    return c;
}

ScreenChoice fullscreen(int ui, int monitor = 0) {
    ScreenChoice c;
    c.show = Show::Fullscreen;
    c.ui = ui;
    c.monitor = monitor;
    return c;
}

}  // namespace

TEST(screen_the_users_window_keeps_the_games_own_window) {
    // In a window, 3360x1890 at 175 % on a 175 % monitor: the game draws 1920x1080, ROExt or uaRO Pad keep the window.
    const ScreenPlan p = planScreen(window(3360, 1890, 175), {fourK()}, Backend::Uaro, {}, {});
    CHECK(p.problem == ScreenProblem::None);
    CHECK(p.game == (Size{1920, 1080}));
    CHECK(!p.fullscreen);
    CHECK(p.writesIni);
    CHECK(p.mode == DisplayMode::Auto);
    CHECK(p.iniWindow == (Size{0, 0}));
    CHECK(!p.writesRoext);
    CHECK(std::abs(p.s - 1.75) < 0.001);
    CHECK(!integerScale(p.s));
}

TEST(screen_a_bigger_interface_in_the_same_window_is_our_window) {
    const ScreenPlan p = planScreen(window(3360, 1890, 200), {fourK()}, Backend::Uaro, {}, {});
    CHECK(p.problem == ScreenProblem::None);
    CHECK(p.game == (Size{1680, 945}));
    CHECK(p.mode == DisplayMode::Window);
    CHECK(p.iniWindow == (Size{1920, 1080}));  // W / d: the window in the game's own pixels
    CHECK(integerScale(p.s));
}

TEST(screen_full_screen_divides_the_monitor_by_the_interface_size) {
    const ScreenPlan a = planScreen(fullscreen(175), {fourK()}, Backend::Uaro, {}, {});
    CHECK(a.problem == ScreenProblem::None);
    CHECK(a.fullscreen);
    CHECK(a.game == (Size{2194, 1234}));
    CHECK(a.mode == DisplayMode::Auto);
    const ScreenPlan b = planScreen(fullscreen(200), {fourK()}, Backend::Uaro, {}, {});
    CHECK(b.game == (Size{1920, 1080}));
    CHECK(integerScale(b.s));
    const ScreenPlan c = planScreen(fullscreen(100), {fourK()}, Backend::Uaro, {}, {});
    CHECK(c.game == (Size{3840, 2160}));  // no limit on the game's size in full screen (registry.md §2)
}

TEST(screen_a_second_monitor_is_our_window_there) {
    // At 100 % on a 100 % second monitor: the game's window is made on the main one, so our window moves it.
    const ScreenPlan p = planScreen(window(1600, 900, 100, 1), {fourK(), fullHd()}, Backend::Uaro, {}, {});
    CHECK(p.problem == ScreenProblem::None);
    CHECK(p.mode == DisplayMode::Window);
    CHECK_EQ(p.monitor, 1);
    CHECK(p.iniWindow == (Size{1600, 900}));
    const ScreenPlan f = planScreen(fullscreen(100, 1), {fourK(), fullHd()}, Backend::Uaro, {}, {});
    CHECK(f.game == (Size{1920, 1080}));
    CHECK_EQ(f.monitor, 1);
}

TEST(screen_refuses_windows_that_do_not_fit) {
    // The window itself is taller than the work area.
    CHECK(planScreen(window(3840, 2160, 175), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::TooBig);
    // The window fits, but the game's own window (2240x1260 logical with its frame) is larger than the main monitor's
    // work area of 2194x1193 logical: Windows would cut it.
    CHECK(planScreen(window(2240, 1260, 100), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::TooBig);
    CHECK(planScreen(window(1024, 768, 300), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::TooSmall);
    CHECK(planScreen(window(0, 768, 175), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::BadNumber);
    CHECK(planScreen(fullscreen(99), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::BadNumber);
    CHECK(planScreen(fullscreen(401), {fourK()}, Backend::Uaro, {}, {}).problem == ScreenProblem::BadNumber);
    CHECK(planScreen(fullscreen(175), {}, Backend::Uaro, {}, {}).problem == ScreenProblem::BadNumber);
}

TEST(screen_dgvoodoo_keeps_windows_scaling_and_real_modes) {
    // A window: the interface size is Windows scaling, whatever was picked; our ini stays as it is.
    const ScreenPlan w = planScreen(window(3360, 1890, 200), {fourK()}, Backend::DgVoodoo, {}, {});
    CHECK(w.problem == ScreenProblem::None);
    CHECK(w.game == (Size{1920, 1080}));
    CHECK(!w.writesIni);
    // Full screen: the monitor mode picked is the game's size.
    ScreenChoice c = fullscreen(175);
    c.mode = {2560, 1440};
    const ScreenPlan f = planScreen(c, {fourK()}, Backend::DgVoodoo, {}, {});
    CHECK(f.fullscreen);
    CHECK(f.game == (Size{2560, 1440}));
    CHECK(!f.writesIni);
}

TEST(screen_roext_gets_the_same_size_in_a_window) {
    RoextWindow roext;
    roext.active = true;
    roext.lock = true;
    CHECK(planScreen(window(3360, 1890, 175), {fourK()}, Backend::Uaro, roext, {}).writesRoext);
    CHECK(!planScreen(fullscreen(175), {fourK()}, Backend::Uaro, roext, {}).writesRoext);
    roext.lock = false;
    CHECK(!planScreen(window(3360, 1890, 175), {fourK()}, Backend::Uaro, roext, {}).writesRoext);
    roext.size = {1600, 863};
    CHECK(planScreen(window(3360, 1890, 175), {fourK()}, Backend::DgVoodoo, roext, {}).writesRoext);
    roext.active = false;  // a renamed ROExt (dinput.dll.bak) reads nothing
    CHECK(!planScreen(window(3360, 1890, 175), {fourK()}, Backend::Uaro, roext, {}).writesRoext);
}

TEST(screen_offers_window_sizes_that_fit) {
    const std::vector<Size> sizes = windowSizes(fourK(), {});
    CHECK(!sizes.empty());
    CHECK(sizes[0] == (Size{3591, 2020}));  // the largest 16:9 window under the task bar
    bool users = false;
    for (Size s : sizes) {
        users = users || s == Size{3360, 1890};
        CHECK(s.width + 28 <= 3840 && s.height + 68 <= 2088);
    }
    CHECK(users);
    const std::vector<Size> small = windowSizes(fullHd(), {});
    CHECK(small[0] == (Size{1780, 1001}));  // 1040 - 39 high, 16:9
    for (Size s : small) CHECK(s != (Size{1920, 1080}));
}

TEST(screen_first_run_takes_the_largest_standard_window) {
    CHECK(defaultWindow(fourK(), {}) == (Size{3360, 1890}));  // 1920x1080 at 175 %
    CHECK(defaultWindow(fullHd(), {}) == (Size{1600, 900}));  // 1920x1080 does not fit above the task bar
    MonitorInfo tiny = fullHd();
    tiny.work = {800, 560};
    CHECK(defaultWindow(tiny, {}) == (Size{0, 0}));
}

TEST(screen_now_reads_what_the_game_will_get) {
    const Config defaults;
    // The user's PC: a window of 1920x1080, mode = auto, uaRO Pad instead of ROExt.
    const ScreenNow a = currentScreen(false, {1920, 1080}, defaults, {}, {fourK()}, Backend::Uaro);
    CHECK(a.choice.show == Show::Window);
    CHECK(a.choice.window == (Size{3360, 1890}));
    CHECK_EQ(a.choice.ui, 175);
    CHECK(a.game == (Size{1920, 1080}));
    CHECK(!a.roextOverride);
    // An active ROExt with its own size is stronger than the registry.
    RoextWindow roext;
    roext.active = true;
    roext.size = {1600, 863};
    const ScreenNow b = currentScreen(false, {1920, 1080}, defaults, roext, {fourK()}, Backend::Uaro);
    CHECK(b.roextOverride);
    CHECK(b.game == (Size{1600, 863}));
    CHECK(b.choice.window == (Size{2800, 1510}));
    // WindowLock without a size covers the work area.
    roext.size = {};
    roext.lock = true;
    CHECK(currentScreen(false, {1920, 1080}, defaults, roext, {fourK()}, Backend::Uaro).game == (Size{2194, 1193}));
    // Full screen.
    const ScreenNow f = currentScreen(true, {2194, 1234}, defaults, {}, {fourK()}, Backend::Uaro);
    CHECK(f.choice.show == Show::Fullscreen);
    CHECK_EQ(f.choice.ui, 175);
    CHECK(f.choice.mode == (Size{2194, 1234}));
    // Our window of 1920x1080 logical around a 1680x945 game.
    Config ours;
    ours.mode = DisplayMode::Window;
    ours.windowWidth = 1920;
    ours.windowHeight = 1080;
    const ScreenNow w = currentScreen(false, {1680, 945}, ours, {}, {fourK()}, Backend::Uaro);
    CHECK(w.choice.window == (Size{3360, 1890}));
    CHECK_EQ(w.choice.ui, 200);
    // planScreen of what the page starts from writes the same again.
    const ScreenPlan again = planScreen(a.choice, {fourK()}, Backend::Uaro, {}, {});
    CHECK(again.game == (Size{1920, 1080}));
    CHECK(again.mode == DisplayMode::Auto);
}

TEST(screen_game_size_in_a_window_is_rounded_as_the_plan) {
    CHECK(gameSize({3360, 1890}, 175) == (Size{1920, 1080}));
    CHECK(gameSize({2560, 1440}, 175) == (Size{1463, 823}));  // 1462.9 x 822.9: rounded, not cut
    ScreenChoice c;
    c.window = {2560, 1440};
    c.ui = 175;
    CHECK(planScreen(c, {fourK()}, Backend::Uaro, RoextWindow{}, Frame{}).game == gameSize(c.window, c.ui));
}
