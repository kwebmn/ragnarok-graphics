// 3.0.18-beta.193 (the owner: «первый экран... новый пользователь может испугаться»; docs/superpowers/plans/
// 2026-11-12-launcher.md Task 3): the launcher's places at any window size and Windows' scale, what is under the mouse,
// what the big button says
#include <initializer_list>

#include "core/launcher_layout.hpp"
#include "test.hpp"

using namespace uagfx;
using launcherview::Box;
using launcherview::Part;

namespace {

bool inside(const Box& b, int w, int h) { return b.x >= 0 && b.y >= 0 && b.x + b.w <= w && b.y + b.h <= h; }
bool apart(const Box& a, const Box& b) {
    return a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y || b.y + b.h <= a.y;
}
float cx(const Box& b) { return b.x + b.w / 2; }
float cy(const Box& b) { return b.y + b.h / 2; }

}  // namespace

TEST(launcher_layout_fits_small_and_scaled_windows) {
    struct Size {
        int w, h;
        float s;
    };
    const Size sizes[] = {Size{960, 560, 1.0f}, Size{1280, 720, 1.0f}, Size{3360, 1890, 1.75f},
                          Size{1680, 945, 1.75f}};
    for (const Size z : sizes) {
        const launcherview::Layout L = launcherview::layoutFor(z.w, z.h, z.s, 3, true);
        for (const Box& b : {L.bar, L.logo, L.server, L.language, L.feed, L.update, L.look, L.play, L.playMore,
                             L.version, L.plate})
            CHECK(inside(b, z.w, z.h));
        for (const Box& d : L.dock) {
            CHECK(inside(d, z.w, z.h));
            CHECK(apart(d, L.play));
            CHECK(apart(d, L.plate));
        }
        for (int i = 0; i < L.newsShown; ++i) {
            CHECK(inside(L.news[i], z.w, z.h));
            CHECK(apart(L.news[i], L.plate));
            CHECK(apart(L.news[i], L.dock[0]));
        }
        CHECK(apart(L.feed, L.play));
        CHECK(apart(L.feed, L.look));
        CHECK(apart(L.server, L.logo));
        CHECK(apart(L.play, L.playMore));
        CHECK(L.newsShown >= 1);  // room for one news card at least, even in the smallest window
    }
}

TEST(launcher_layout_without_news) {
    const launcherview::Layout L = launcherview::layoutFor(1280, 720, 1.0f, 0, false);
    CHECK_EQ(L.newsShown, 0);
    CHECK(L.update.w > 0 && L.update.h > 0);  // the update card stays
    CHECK(L.plate.w == 0);                    // no plate asked
    const launcherview::Layout M = launcherview::layoutFor(1280, 720, 1.0f, 7, false);
    CHECK(M.newsShown <= 3);
}

TEST(launcher_layout_hits) {
    const launcherview::Layout L = launcherview::layoutFor(1280, 720, 1.0f, 2, true);
    CHECK(launcherview::hitAt(L, cx(L.play), cy(L.play)).part == Part::Play);
    CHECK(launcherview::hitAt(L, cx(L.playMore), cy(L.playMore)).part == Part::PlayMore);
    CHECK(launcherview::hitAt(L, cx(L.look), cy(L.look)).part == Part::Look);
    CHECK(launcherview::hitAt(L, cx(L.update), cy(L.update)).part == Part::Changelog);
    const launcherview::Hit news = launcherview::hitAt(L, cx(L.news[1]), cy(L.news[1]));
    CHECK(news.part == Part::News);
    CHECK_EQ(news.index, 1);
    CHECK(launcherview::hitAt(L, cx(L.dock[0]), cy(L.dock[0])).part == Part::Settings);
    CHECK(launcherview::hitAt(L, cx(L.dock[1]), cy(L.dock[1])).part == Part::Reports);
    CHECK(launcherview::hitAt(L, cx(L.dock[2]), cy(L.dock[2])).part == Part::Site);
    CHECK(launcherview::hitAt(L, cx(L.dock[3]), cy(L.dock[3])).part == Part::Forum);
    CHECK(launcherview::hitAt(L, cx(L.version), cy(L.version)).part == Part::Version);
    CHECK(launcherview::hitAt(L, cx(L.language), cy(L.language)).part == Part::Language);
    CHECK(launcherview::hitAt(L, cx(L.plate), cy(L.plate)).part == Part::Plate);
    CHECK(launcherview::hitAt(L, 700.0f, 300.0f).part == Part::None);  // the art itself
}

TEST(launcher_play_state_running_wins) {
    using launcherview::PlayInputs;
    using launcherview::PlayState;
    PlayInputs in;
    CHECK(launcherview::playState(in) == PlayState::Play);
    in.offered = true;
    CHECK(launcherview::playState(in) == PlayState::Update);
    in.downloading = true;
    CHECK(launcherview::playState(in) == PlayState::Updating);
    in.gameRunning = true;  // never an update over a running game: another window
    CHECK(launcherview::playState(in) == PlayState::Running);
}
