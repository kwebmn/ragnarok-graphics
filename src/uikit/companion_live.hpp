#pragma once
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/homun_card.hpp"
#include "core/merc_card.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::companion {

// 3.0.18-beta.98 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md): the homunculus' and the mercenary's cards
// laid out by RmlUi - one markup: the name and level, the bars (HP, SP, experience, fullness), the facts (closeness;
// the contract, loyalty, summons, kills), the eight stats two a line, the homunculus' «Нагодувати», the skills (name,
// ‹ level ›, the homunculus' «+»), «Видалити» / «Звільнити». refit*() put RmlUi's boxes into the cards' core layouts.
struct BarV {
    Rml::String label, value, cls;
    float fill = 0;  // %
};
struct FactV {
    Rml::String label, value, cls = "fv";
};
struct StatV {
    Rml::String name, value;
};
struct SkillV {
    Rml::String name, level;
    bool arrows = false, plus = false, below = false, hot = false, pad = false;
    bool lessHot = false, moreHot = false, plusHot = false, dash = false;
};
struct Model {
    Rml::String ground = "#00000000", title, name, level, feed, skillsHead, remove;
    bool closeHot = false, feedHot = false, feedFocus = false, removeHot = false, raise = false, headFocus = false;
    std::vector<BarV> bars;
    std::vector<FactV> facts;
    std::vector<StatV> stats;
    std::vector<SkillV> skills;
};

const char* rml();
void bind(Rml::DataModelConstructor& c, Model& m);
void fillHomun(Model& m, const homuncard::Info& i, const homuncard::Show& sh, const homuncard::Texts& t);
void fillMerc(Model& m, const merccard::Info& i, const merccard::Show& sh, const merccard::Texts& t);
bool refitHomun(Rml::ElementDocument* doc, homuncard::Layout& L);
bool refitMerc(Rml::ElementDocument* doc, merccard::Layout& L);

}  // namespace uagfx::uikit::companion
