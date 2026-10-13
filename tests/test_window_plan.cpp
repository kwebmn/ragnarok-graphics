#include "core/config.hpp"
#include "core/window_plan.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
Config withMode(DisplayMode m) {
    Config c;
    c.mode = m;
    return c;
}
}  // namespace

// The shipped client runs windowed, sized by ROExt (1600x863): with mode = auto its window is left alone and the
// picture is shown 1:1, as with dgVoodoo.
TEST(winplan_auto_keeps_a_windowed_games_window) {
    CHECK(windowAction(Config{}, false) == WindowAction::Leave);
}

TEST(winplan_auto_turns_full_screen_into_borderless) {
    CHECK(windowAction(Config{}, true) == WindowAction::Borderless);
}

TEST(winplan_auto_with_a_window_size_makes_a_window) {
    Config c;
    c.windowWidth = 1280;
    CHECK(windowAction(c, false) == WindowAction::Window);
}

TEST(winplan_explicit_modes_override_the_game) {
    CHECK(windowAction(withMode(DisplayMode::Window), true) == WindowAction::Window);
    CHECK(windowAction(withMode(DisplayMode::Borderless), false) == WindowAction::Borderless);
}

TEST(fit_same_size_is_one_to_one) {
    CHECK(fitPicture(1600, 863, 1600, 863) == (Rect{0, 0, 1600, 863}));
}

TEST(fit_offsets_are_whole_pixels) {
    // A half-pixel offset would make every output row sample between two source rows: the whole picture blurs.
    CHECK(fitPicture(1600, 863, 1600, 900) == (Rect{0, 18, 1600, 881}));
}

TEST(fit_scales_keeping_the_shape) {
    CHECK(fitPicture(1600, 863, 1920, 1080) == (Rect{0, 22, 1920, 1058}));
    CHECK(fitPicture(1024, 768, 1920, 1080) == (Rect{240, 0, 1680, 1080}));
    CHECK(fitPicture(800, 600, 1600, 1200) == (Rect{0, 0, 1600, 1200}));
}

TEST(fit_nothing_for_empty_sizes) {
    CHECK(fitPicture(0, 600, 1920, 1080).empty());
    CHECK(fitPicture(800, 600, 0, 0).empty());
}
