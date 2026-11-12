// 3.0.18-beta.193 (the owner: «первый экран... новый пользователь может испугаться»; docs/superpowers/plans/
// 2026-11-12-launcher.md Task 3): the launcher's places at any window size and Windows' scale, what is under the mouse,
// what the big button says
#include <cmath>
#include <initializer_list>

#include "core/launcher_layout.hpp"
#include "core/launcher_log.hpp"
#include "core/settings/choice_presets.hpp"
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
    // 3.0.18-beta.202 (the owner: «кнопку проверить обновления рядом с кнопкой играть», «кнопку выбора канала»): the
    // check beside the big button, the channel's two halves beside the version
    CHECK(launcherview::hitAt(L, cx(L.check), cy(L.check)).part == Part::Check);
    const launcherview::Hit beta = launcherview::hitAt(L, cx(L.channel[1]), cy(L.channel[1]));
    CHECK(beta.part == Part::Channel && beta.index == 1);
    CHECK(launcherview::hitAt(L, cx(L.channel[0]), cy(L.channel[0])).index == 0);
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

// 3.0.18-beta.196 (Task 9): the first start - the four players' cards side by side over the launcher, «Пропустити»
// under them; the card for the computer's graphics card marked
TEST(launcher_first_run_cards) {
    struct Size {
        int w, h;
        float s;
    };
    const Size sizes[] = {Size{960, 560, 1.0f}, Size{1280, 720, 1.0f}, Size{3360, 1890, 1.75f}};
    for (const Size z : sizes) {
        const launcherview::Layout L = launcherview::layoutFor(z.w, z.h, z.s, 3, false);
        const launcherview::FirstRun f = launcherview::firstRunFor(L);
        const Box client{0, 0, float(z.w), float(z.h)};
        for (int i = 0; i < launcherview::kKindCards; ++i) {
            const Box& c = f.cards[i];
            CHECK(c.w > 0 && c.h > c.w * 9.0f / 16.0f);  // a picture and words under it
            CHECK(c.x >= client.x && c.y >= L.bar.h && c.x + c.w <= client.w && c.y + c.h <= client.h);
            if (i) {
                CHECK(c.x >= f.cards[i - 1].x + f.cards[i - 1].w);  // side by side, the same row
                CHECK(c.y == f.cards[0].y && c.w == f.cards[0].w);
            }
            CHECK_EQ(launcherview::firstRunAt(f, c.x + c.w / 2, c.y + c.h / 2), i);
        }
        CHECK(f.title.y + f.title.h <= f.note.y);
        // 3.0.18-beta.199 (the owner: «автоопределение видеокарты надо делать сразу при первом запуске»): what was
        // chosen for the card at once, between the note and the cards
        CHECK(f.found.w > 0 && f.found.y >= f.note.y + f.note.h && f.found.y + f.found.h <= f.cards[0].y);
        CHECK(f.skip.y >= f.cards[0].y + f.cards[0].h && f.skip.y + f.skip.h <= client.h);
        CHECK_EQ(launcherview::firstRunAt(f, f.skip.x + f.skip.w / 2, f.skip.y + f.skip.h / 2),
                 launcherview::kKindCards);
        CHECK_EQ(launcherview::firstRunAt(f, 1, 1), -1);
    }
    // the card marked «для вашої відеокарти»: everything new on a card that draws it, the weak computer's on a weak
    // one, an integrated one or one of little memory
    namespace presets = settings::presets;
    CHECK(presets::suggestedKind(3, 8192, false) == presets::Kind::AllNew);
    CHECK(presets::suggestedKind(2, 4096, false) == presets::Kind::AllNew);
    CHECK(presets::suggestedKind(1, 4096, false) == presets::Kind::WeakPc);
    CHECK(presets::suggestedKind(3, 8192, true) == presets::Kind::WeakPc);
    CHECK(presets::suggestedKind(3, 1024, false) == presets::Kind::WeakPc);
}

// 3.0.18-beta.199 (the owner: «можно было запустить и без обновления и это не было спрятано»): an update offered - the
// big button updates and plays, «Грати без оновлення» in sight under it, over the version
TEST(launcher_layout_update_offered_shows_play_without_it) {
    struct Size {
        int w, h;
        float s;
    };
    const Size sizes[] = {Size{960, 560, 1.0f}, Size{1280, 720, 1.0f}, Size{3360, 1890, 1.75f}};
    for (const Size z : sizes) {
        const launcherview::Layout L = launcherview::layoutFor(z.w, z.h, z.s, 3, false, true);
        CHECK(L.noUpdate.w > 0 && L.noUpdate.h > 0);
        CHECK(L.noUpdate.y >= L.play.y + L.play.h);           // under the big button
        CHECK(L.noUpdate.y + L.noUpdate.h <= L.version.y);    // over the version
        CHECK(L.look.y + L.look.h <= L.play.y);
        CHECK(L.look.y >= L.bar.y + L.bar.h);
        CHECK(L.noUpdate.x >= L.check.x && L.noUpdate.x + L.noUpdate.w <= L.playMore.x + L.playMore.w + 0.01f);
        const launcherview::Hit h =
            launcherview::hitAt(L, L.noUpdate.x + L.noUpdate.w / 2, L.noUpdate.y + L.noUpdate.h / 2);
        CHECK(h.part == Part::NoUpdate);
    }
    const launcherview::Layout plain = launcherview::layoutFor(1280, 720, 1.0f, 3, false);  // nothing offered
    CHECK_EQ(plain.noUpdate.w, 0.0f);
    CHECK(launcherview::hitAt(plain, 1100, plain.version.y - 20).part != Part::NoUpdate);
}

// 3.0.18-beta.202: «Перевірити» a square beside «ГРАТИ», the same height; «Реліз | Бета» left of the version in its row
// 3.0.18-beta.203 (the owner: «лого на картинке с заставки самого клиента взять»): the logo cut out of the client's
// login picture at the top left, taller than the bar, the feed under it; two news still fit the smallest window
TEST(launcher_layout_logo_above_the_feed) {
    struct Size {
        int w, h;
        float s;
    };
    const Size sizes[] = {Size{960, 560, 1.0f}, Size{1280, 720, 1.0f}, Size{3360, 1890, 1.75f}};
    for (const Size z : sizes) {
        const launcherview::Layout L = launcherview::layoutFor(z.w, z.h, z.s, 3, false);
        CHECK(L.logo.h >= 72 * z.s && L.logo.h > L.bar.h);
        const float aspect = L.logo.w / L.logo.h;  // the picture's own: 491 x 206
        CHECK(aspect > 2.33f && aspect < 2.43f);
        CHECK(L.logo.x >= 0 && L.logo.y >= 0);
        CHECK(L.logo.y + L.logo.h <= L.feed.y);
        CHECK(L.logo.x + L.logo.w <= L.server.x);
        const launcherlog::Screen log =
            launcherlog::layoutFor(L, {}, launcherlog::Filter::All, "", "2026-11-12", 0, settings::Lang::Uk);
        CHECK(log.back.y >= L.logo.y + L.logo.h);  // «Журнал змін»'s «← До гри» under it
        CHECK(launcherview::firstRunFor(L).title.y >= L.logo.y + L.logo.h);
    }
    CHECK_EQ(launcherview::layoutFor(960, 560, 1.0f, 3, false).newsShown, 2);
}

// 3.0.18-beta.205 (the owner: «надо еще в лаунчере сделать кнопку создать аккаунт»): «Створити акаунт» in the top
// bar, left of the server's pill - the game started with its registration open
TEST(launcher_layout_account_in_the_bar) {
    struct Size {
        int w, h;
        float s;
    };
    const Size sizes[] = {Size{960, 560, 1.0f}, Size{1280, 720, 1.0f}, Size{3360, 1890, 1.75f}};
    for (const Size z : sizes) {
        const launcherview::Layout L = launcherview::layoutFor(z.w, z.h, z.s, 3, false);
        CHECK(L.account.w > 0 && L.account.h > 0);
        CHECK(L.account.y >= L.bar.y && L.account.y + L.account.h <= L.bar.y + L.bar.h);
        CHECK(L.account.x + L.account.w <= L.server.x);
        CHECK(L.account.x >= L.logo.x + L.logo.w);
        const launcherview::Hit h =
            launcherview::hitAt(L, L.account.x + L.account.w / 2, L.account.y + L.account.h / 2);
        CHECK(h.part == launcherview::Part::Account);
    }
}

TEST(launcher_layout_check_and_channel_by_the_button) {
    struct Size {
        int w, h;
        float s;
    };
    const Size sizes[] = {Size{960, 560, 1.0f}, Size{1280, 720, 1.0f}, Size{3360, 1890, 1.75f}};
    for (const Size z : sizes)
        for (const bool offered : {false, true}) {
            const launcherview::Layout L = launcherview::layoutFor(z.w, z.h, z.s, 3, false, offered);
            CHECK(L.check.w > 0 && L.check.h == L.play.h && L.check.y == L.play.y);
            // 3.0.18-beta.203 (the owner: «кнопку обновить как-то в кнопку "играть" совместить»): the big button's
            // left part, ▾ its right one - one button across the play column
            CHECK(std::fabs(L.check.x + L.check.w - L.play.x) < 0.01f);
            CHECK(std::fabs(L.play.x + L.play.w - L.playMore.x) < 0.01f);
            CHECK(std::fabs(L.check.x - L.look.x) < 0.01f);
            CHECK(std::fabs(L.playMore.x + L.playMore.w - (L.look.x + L.look.w)) < 0.01f);
            CHECK(L.channel[0].x + L.channel[0].w <= L.channel[1].x);
            CHECK(L.channel[1].x + L.channel[1].w <= L.version.x);
            CHECK(L.channel[0].y == L.version.y && L.channel[0].h > 0);
            CHECK(L.channel[0].x >= L.check.x);  // in the play column
            CHECK(L.version.x + L.version.w <= float(z.w));
            CHECK(L.play.y + L.play.h <= (offered ? L.noUpdate.y : L.version.y));
            CHECK(L.look.y + L.look.h <= L.play.y);
        }
}

// 3.0.18-beta.211 (the keyboard concept, stage 7): the arrows by place on the first screen - the nearest part that way
// (its centre within 45° of it, the distance across the way weighed double); none past the edge
TEST(launcher_layout_arrows_by_place) {
    using launcherview::Box;
    // three in a row, one under the middle
    const std::vector<Box> b{Box{0, 0, 40, 20}, Box{60, 0, 40, 20}, Box{120, 0, 40, 20}, Box{60, 80, 40, 20}};
    CHECK_EQ(launcherview::nearestToward(b, 0, 1, 0), 1);   // right of the first: the middle
    CHECK_EQ(launcherview::nearestToward(b, 1, 1, 0), 2);
    CHECK_EQ(launcherview::nearestToward(b, 2, 1, 0), -1);  // the edge
    CHECK_EQ(launcherview::nearestToward(b, 1, 0, 1), 3);   // down from the middle
    CHECK_EQ(launcherview::nearestToward(b, 3, 0, -1), 1);  // and back up
    CHECK_EQ(launcherview::nearestToward(b, 0, 0, 1), 3);   // down from the first: the one under, though not straight
    CHECK_EQ(launcherview::nearestToward(b, 3, -1, 0), -1); // nothing left of the lone one within 45°
    CHECK_EQ(launcherview::nearestToward(b, -1, 1, 0), -1); // no part to start from
}
