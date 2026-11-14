// 3.0.11-beta.198 (the tester: «курсор миші стандартний не дуже підходить в загальний стиль»; the owner: «да, сделай
// возможность курсор менять»): the cursor in the interface's style (core/cursor_art) - the game's cursor actions as
// its shapes, each drawn at any size with its point where the game's cursor is
#include <cmath>

#include <string>

#include "core/config.hpp"
#include "core/cursor_art.hpp"
#include "core/live_config.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

uint32_t at(const panel::Picture& p, float x, float y) {
    return p.px[size_t(int(y)) * size_t(p.width) + size_t(int(x))];
}
int alphaOf(uint32_t c) { return int(c >> 24); }

}  // namespace

// cursors.act's 14 actions: the arrow, talk, a window's button, a gem falling onto the arrow, the camera turned,
// attack (two), a warp (two), not there, pick up, a target (two), the arrow again
TEST(cursor_art_shapes_of_the_games_actions) {
    using S = cursorart::Shape;
    const S want[14] = {S::Arrow,   S::Talk, S::Hand,    S::Arrow, S::Rotate, S::Attack, S::Attack,
                        S::Door,    S::Invalid, S::Grab, S::Target, S::Target, S::Door, S::Arrow};
    for (int a = 0; a < 14; ++a) CHECK(cursorart::shapeOf(a) == want[a]);
    CHECK(cursorart::shapeOf(-1) == S::Arrow && cursorart::shapeOf(99) == S::Arrow);
}

// 3.0.19-beta.10 (the 3D view's crosshair, win/view3d): the game's own action at the middle of the screen - over a
// monster a red target, an NPC's talk, a thing to pick up, a warp and «not there» as the game's; the rest the target
TEST(cursor_art_the_3d_crosshair_by_the_games_action) {
    using S = cursorart::Shape;
    const S want[14] = {S::Target, S::Talk,  S::Hand,    S::Target, S::Target, S::TargetHot, S::TargetHot,
                        S::Door,   S::Invalid, S::Grab, S::Target, S::Target, S::Door,      S::Target};
    for (int a = 0; a < 14; ++a) CHECK(cursorart::lookShapeOf(a) == want[a]);
    CHECK(cursorart::lookShapeOf(-1) == S::Target && cursorart::lookShapeOf(99) == S::Target);
}

// The red target: the target's rings in red (in both looks), its point in the middle
TEST(cursor_art_the_hot_target_is_red) {
    const auto redPixels = [](const cursorart::Art& a) {
        int n = 0;
        for (const uint32_t c : a.picture.px) {
            const int al = alphaOf(c), r = int((c >> 16) & 255), g = int((c >> 8) & 255);
            n += al > 200 && r > 150 && g < 110;
        }
        return n;
    };
    for (const CursorLook look : {CursorLook::Dark, CursorLook::Light}) {
        const cursorart::Art hot = cursorart::make(cursorart::Shape::TargetHot, look, 24);
        const cursorart::Art plain = cursorart::make(cursorart::Shape::Target, look, 24);
        CHECK(redPixels(hot) > 20);
        CHECK(redPixels(plain) == 0);
        CHECK(hot.picture.width == plain.picture.width && hot.hotX == plain.hotX && hot.hotY == plain.hotY);
    }
}

// The arrow: its point at the picture's top left (where the game's is), dark glass inside a gold edge («Темний»),
// white inside a dark one («Світлий»), see-through away from it; twice the size twice as big
TEST(cursor_art_arrow_looks) {
    const cursorart::Art dark = cursorart::make(cursorart::Shape::Arrow, CursorLook::Dark, 24);
    CHECK(dark.picture.width >= 24 && dark.picture.height >= 24);
    CHECK(dark.hotX >= 0 && dark.hotX < float(dark.picture.width) / 4);  // its point in the top left (a margin
    CHECK(dark.hotY >= 0 && dark.hotY < float(dark.picture.height) / 4);  // round it for the shadow)
    const uint32_t inside = at(dark.picture, dark.hotX + 4, dark.hotY + 9);
    CHECK(alphaOf(inside) > 230 && (inside & 0xFF) < 0x40);  // dark glass
    int gold = 0;  // its edge along a row through it
    for (int x = 0; x < dark.picture.width; ++x) {
        const uint32_t c = at(dark.picture, float(x), dark.hotY + 9);
        gold += alphaOf(c) > 200 && ((c >> 16) & 0xFF) > 0xC0 && (c & 0xFF) < 0xA0;
    }
    CHECK(gold > 0);
    CHECK(alphaOf(at(dark.picture, float(dark.picture.width - 1), dark.hotY)) == 0);  // beside its point: nothing
    const cursorart::Art light = cursorart::make(cursorart::Shape::Arrow, CursorLook::Light, 24);
    const uint32_t white = at(light.picture, light.hotX + 4, light.hotY + 9);
    CHECK(alphaOf(white) > 230 && (white & 0xFF) > 0xE0);
    const cursorart::Art big = cursorart::make(cursorart::Shape::Arrow, CursorLook::Dark, 48);
    CHECK(std::abs(big.picture.width - 2 * dark.picture.width) <= 2);
    CHECK(std::fabs(big.hotX - 2 * dark.hotX) < 1.5f && std::fabs(big.hotY - 2 * dark.hotY) < 1.5f);
}

// The shapes round their point (talk, the camera turned, a warp, not there, pick up, a target): the point in the
// picture's middle; every shape drawn, none empty
TEST(cursor_art_every_shape_drawn) {
    for (int i = 0; i < int(cursorart::Shape::kCount); ++i) {
        const auto shape = cursorart::Shape(i);
        const cursorart::Art a = cursorart::make(shape, CursorLook::Dark, 24);
        CHECK(a.picture.width > 0 && a.picture.height > 0);
        CHECK(a.hotX >= 0 && a.hotY >= 0 && a.hotX < float(a.picture.width) && a.hotY < float(a.picture.height));
        int seen = 0;
        for (const uint32_t c : a.picture.px) seen += alphaOf(c) > 200;
        CHECK(seen > 20);
        if (shape == cursorart::Shape::Target || shape == cursorart::Shape::TargetHot ||
            shape == cursorart::Shape::Invalid || shape == cursorart::Shape::Rotate)
            CHECK(std::fabs(a.hotX - float(a.picture.width) / 2) < 3 &&
                  std::fabs(a.hotY - float(a.picture.height) / 2) < 3);
    }
}

// [interface] cursor - the game's own (by default), «Темний», «Світлий»; a choice of «Інтерфейс» applied at once
TEST(config_cursor_look) {
    CHECK(parseConfig("").cursorLook == CursorLook::Game);
    const char* tokens[] = {"game", "dark", "light"};
    const CursorLook looks[] = {CursorLook::Game, CursorLook::Dark, CursorLook::Light};
    for (int i = 0; i < 3; ++i) {
        CHECK(parseConfig(std::string("[interface]\ncursor = ") + tokens[i] + "\n").cursorLook == looks[i]);
        CHECK(std::string(cursorLookToken(looks[i])) == tokens[i]);
    }
    CHECK(parseConfig("[interface]\ncursor = rainbow\n").cursorLook == CursorLook::Game);
}

// The choice in «Інтерфейс», applied at once
TEST(config_cursor_look_in_the_app) {
    const settings::IniSetting* s = settings::findSetting("interface.cursor");
    CHECK(s != nullptr && s->page == settings::Page::Interface && s->control == settings::Control::Choice);
    CHECK(appliesNow("interface.cursor"));
    CHECK(liveUpdate(parseConfig(""), parseConfig("[interface]\ncursor = dark\n")).next.cursorLook == CursorLook::Dark);
}
