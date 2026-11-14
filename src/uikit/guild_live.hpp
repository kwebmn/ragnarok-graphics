#pragma once
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/guild_card.hpp"
#include "uikit/layered.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::guild {

// 3.0.18-beta.102 (docs/superpowers/plans/2026-11-10-uaui-live-drag.md, group 3): the guild's card live - its head
// (the title, ×) and its six tabs (two rows of three, a tab not allowed dimmed) by RmlUi, its body layered
// (uikit/layered): the facts, the members' rows with their dots, the positions, skills with «+», the notice and its
// fields, «Змінити» / «Скасувати» keep core guildcard's places, the pressable ones elements of their own.
struct TabV {
    Rml::String name;
    bool on = false, hot = false, off = false;
};
struct Model {
    float base = 12, width = 0;
    Rml::String ground = "#00000000", title;
    bool closeHot = false;
    std::vector<TabV> tabs;
    layered::Layer layer;
};

const char* rml();
void bind(Rml::DataModelConstructor& c, Model& m);
// 3.0.19-beta.1: `emblem` - the guild's emblem picture's source (LivePart::icon), empty: a quiet frame in its place
void fill(Model& m, const guildcard::Layout& L, const guildcard::Info& g, const std::vector<guildcard::Member>& members,
          const guildcard::More& more, const guildcard::Show& sh, const guildcard::Texts& t,
          const std::string& emblem = {});
bool refit(Rml::ElementDocument* doc, guildcard::Layout& L);

}  // namespace uagfx::uikit::guild
