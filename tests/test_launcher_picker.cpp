// 3.0.18-beta.194 (docs/superpowers/plans/2026-11-12-launcher.md Task 7): the launcher's look picker - a card over the
// play column: «Авто для вашої відеокарти», the three choices' rows (every variant of each), the test, all the settings
#include <cstdint>
#include <vector>

#include "core/launcher_layout.hpp"
#include "core/launcher_picker.hpp"
#include "core/settings/choice_presets.hpp"
#include "test.hpp"

using namespace uagfx;
using launcherpick::PickPart;
namespace presets = settings::presets;

namespace {

bool inside(const launcherview::Box& outer, const launcherview::Box& b) {
    return b.w > 0 && b.h > 0 && b.x >= outer.x && b.y >= outer.y && b.x + b.w <= outer.x + outer.w + 0.01f &&
           b.y + b.h <= outer.y + outer.h + 0.01f;
}

}  // namespace

TEST(launcher_picker_rows) {
    struct Size {
        int w, h;
        float s;
    };
    const Size sizes[] = {Size{960, 560, 1.0f}, Size{1280, 720, 1.0f}, Size{3360, 1890, 1.75f}};
    for (const Size z : sizes) {
        const launcherview::Layout L = launcherview::layoutFor(z.w, z.h, z.s, 3, false);
        const launcherpick::Picker p = launcherpick::layoutFor(L);
        // every variant of each choice: «Картинка» 4, «Інтерфейс» 3, «Текстури» 3, in the choices' order
        CHECK_EQ(p.options.size(), size_t(10));
        int perChoice[presets::kChoices] = {};
        for (const launcherpick::Option& o : p.options) ++perChoice[int(o.choice)];
        for (int c = 0; c < presets::kChoices; ++c) CHECK_EQ(perChoice[c], presets::variants(presets::Choice(c)));
        CHECK(p.options.front().choice == presets::Choice::Picture && p.options.front().variant == 0);
        CHECK(p.options.back().choice == presets::Choice::Textures && p.options.back().variant == 2);
        // the card in the window under the top bar; its parts in it
        const launcherview::Box client{0, 0, float(z.w), float(z.h)};
        CHECK(inside(client, p.card));
        CHECK(p.card.y >= L.bar.y + L.bar.h);
        for (const launcherview::Box& b : {p.autoRow, p.test, p.undo, p.all, p.close}) CHECK(inside(p.card, b));
        for (size_t i = 0; i < p.options.size(); ++i) {
            const launcherview::Box& b = p.options[i].box;
            CHECK(inside(p.card, b));
            CHECK_EQ(launcherpick::optionAt(p, b.x + b.w / 2, b.y + b.h / 2), int(i));
            const launcherpick::PickHit h = launcherpick::pickAt(p, b.x + b.w / 2, b.y + b.h / 2);
            CHECK(h.part == PickPart::Option && h.option == int(i));
        }
        // the rows apart: a picture's option above the interface's, the interface's above the textures'
        CHECK(p.options[0].box.y + p.options[0].box.h <= p.options[4].box.y);
        CHECK(p.options[4].box.y + p.options[4].box.h <= p.options[7].box.y);
    }
}

TEST(launcher_picker_hits) {
    const launcherview::Layout L = launcherview::layoutFor(1280, 720, 1.0f, 3, false);
    const launcherpick::Picker p = launcherpick::layoutFor(L);
    const auto centre = [&](const launcherview::Box& b) {
        return launcherpick::pickAt(p, b.x + b.w / 2, b.y + b.h / 2);
    };
    CHECK(centre(p.autoRow).part == PickPart::Auto);
    CHECK(centre(p.test).part == PickPart::Test);
    CHECK(centre(p.undo).part == PickPart::Undo);
    CHECK(centre(p.all).part == PickPart::All);
    CHECK(centre(p.close).part == PickPart::Close);
    CHECK(launcherpick::pickAt(p, p.card.x + 2, p.card.y + p.card.h - 2).part == PickPart::Card);  // its own ground
    CHECK(launcherpick::pickAt(p, 2, 2).part == PickPart::None);                                  // outside: it closes
    CHECK_EQ(launcherpick::optionAt(p, 2, 2), -1);
}
