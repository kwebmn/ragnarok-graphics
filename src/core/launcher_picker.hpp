#pragma once
#include <cstdint>
#include <vector>

#include "core/launcher_layout.hpp"
#include "core/settings/choice_presets.hpp"

namespace uagfx::launcherpick {

// 3.0.18-beta.194 (docs/superpowers/specs/2026-11-12-launcher-design.md «The look picker»): the launcher's card for
// how the game looks, opened by the look line - «Авто для вашої відеокарти» on top, then «Картинка» (with pictures),
// «Інтерфейс», «Текстури» (every variant of core/settings/choice_presets), «Перевірити відеокарту», «Скасувати» (a
// pick's 5 s), «Усі налаштування ›». Over the play column, its right edge on the column's, above the look line.

using launcherview::Box;

struct Option {
    settings::presets::Choice choice = settings::presets::Choice::Picture;
    int variant = 0;
    Box box;
};
struct Picker {
    Box card, title, close, autoRow;
    Box heads[settings::presets::kChoices];  // each row's words
    std::vector<Option> options;             // the choices' variants in order: «Картинка» first
    Box test, undo, all;                     // the card's bottom row
    float s = 1;
};

Picker layoutFor(const launcherview::Layout& L);
// The option under (x, y), -1 none
int optionAt(const Picker& p, float x, float y);

enum class PickPart : uint8_t { None, Card, Option, Auto, Test, Undo, All, Close };
struct PickHit {
    PickPart part = PickPart::None;
    int option = -1;
};
// None: outside the card (a click there closes it); Card: its ground
PickHit pickAt(const Picker& p, float x, float y);

}  // namespace uagfx::launcherpick
