#pragma once
#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/guild_card.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::guild {

// 3.0.22-beta.26 (docs/superpowers/plans/2026-11-19-guild-window.md, task 5): «Гільдія» by hand in RmlUi as
// «Навички» is - the head (the emblem, the name, the level and who is in the game, Alt+G, ×), six tabs underlined (the
// members' count, the master's points), the body (the tab's list, the side panel), the foot (points or the role's
// line; the mouse's hints or the controller's buttons). Every list item a plain wrapper (its views a level down), so
// the lists are exact. refit() puts the boxes of every data-hit into core guildcard's layout.
struct TabV {
    Rml::String name, count, badge;
    bool on = false, hot = false, off = false;
};
struct PairV {  // two words and two marks: a level line, a need, a fact, a person, a column's head
    Rml::String a, b;
    bool on = false, hot = false;
};
struct SkillRowV {
    int id = 0;
    Rml::String group, groupMore, icon, name, kind, info, level;
    float iconW = 0, iconH = 0, op = 1;
    bool active = false, locked = false, learned = false, focus = false, hot = false, plus = false, plusHot = false;
    std::vector<int> dots;
};
struct SideV {
    int id = 0;
    Rml::String icon, name, kind, about, you, where, cool, wait, raiseWord, useWord, who;
    Rml::String youLabel, levelsLabel, whereLabel, coolLabel, needsLabel, whoLabel;
    float iconW = 0, iconH = 0;
    bool active = false, ready = false, use = false, useOff = false, raise = false, useHot = false, raiseHot = false;
    std::vector<PairV> levels, needs;
};
struct GiftV {
    Rml::String icon, what, from;
    float iconW = 0, iconH = 0;
};
struct InfoV {
    Rml::String emblem, name, sub, expLine, alliesLabel, enemiesLabel, noticeLabel, giftsLabel, allWord, none;
    Rml::String subject, body, onlineHead, offline;
    float emblemW = 0, expShare = 0;
    bool alliances = false, allHot = false;
    std::vector<PairV> facts, online;
    std::vector<Rml::String> allies, enemies;
    std::vector<GiftV> gifts;
};
struct MemberRowV {
    int ch = 0;
    Rml::String name, job, level, position, given;
    bool online = false, focus = false, hot = false, me = false;
};
struct ActionV {
    int i = 0;
    Rml::String name;
    bool hot = false, danger = false;
};
struct MembersV {
    Rml::String count, shownSearch, cardName, cardSub, cardPosition, cardGiven, positionLabel, givenLabel, rightsNote;
    bool typing = false, caret = false, searchHot = false, searched = false, card = false, scroll = false;
    float thumbTop = 0, thumbH = 0;
    std::vector<PairV> heads;  // always five
    std::vector<MemberRowV> rows;
    std::vector<ActionV> actions;
};
struct PositionRowV {
    int id = 0;
    Rml::String name, tax, people, r1, r2;
    bool focus = false, hot = false;
};
struct PositionsV {
    Rml::String folded, sideName, sideRights, sideTax, sidePeople, editWord, editNote, noRights, rightsLabel, taxLabel;
    bool canEdit = false, editHot = false;
    std::vector<PairV> heads;  // always four
    std::vector<PositionRowV> rows;
};
struct ExpelledRowV {
    int i = 0;
    Rml::String name, reason;
    bool focus = false, hot = false;
};
struct ExpelledV {
    Rml::String head, nameLabel, accountLabel, reasonLabel, sideName, sideAccount, sideReason, none;
    std::vector<ExpelledRowV> rows;
};
struct NoticeV {
    Rml::String subject, body, preSubject, preBody, subjectCount, bodyCount, fieldTitle, fieldBody, hint, previewLabel;
    Rml::String previewNote, saveWord, cancelWord, editWord, label, none;
    bool editing = false, canEdit = false, empty = false, caret = false, saveHot = false, cancelHot = false;
    bool editHot = false;
    int field = 0;
};
struct PadV {
    Rml::String glyph, words;
    float w = 0, h = 0;
};
struct Model {
    float base = 12, emblemW = 0;
    Rml::String ground = "#00000000", emblem, name, sub, hotkey, footLeft;
    bool closeHot = false, footGold = false;
    // 3.0.22-beta.27: no guild - how to join and how to make one (Alt+G)
    bool noGuild = false;
    Rml::String ngHead, ngSub, ngJoin, ngJoinText, ngMake, ngMakeText;
    int tab = 0;
    std::vector<TabV> tabs;
    std::vector<SkillRowV> skills;
    SideV side;
    InfoV info;
    MembersV members;
    PositionsV positions;
    ExpelledV expelled;
    NoticeV notice;
    std::vector<Rml::String> hints;
    std::vector<PadV> pad;
};

// What the window shows now: core guildcard's views and the head's words
struct Frame {
    float scale = 1;
    std::string name, sub, hotkey;
    bool closeHot = false;
    std::vector<TabV> tabs;
    guildcard::Tab tab = guildcard::Tab::Info;
    guildcard::SkillsView skills;
    guildcard::InfoView info;
    guildcard::MembersView members;
    guildcard::PositionsView positions;
    guildcard::ExpelledView expelled;
    guildcard::NoticeView notice;
    guildcard::FootV foot;
    bool typing = false, caret = false, searchHot = false, allHot = false;
    bool noGuild = false;  // 3.0.22-beta.27: its words from the texts below
    std::string ngHead, ngSub, ngJoin, ngJoinText, ngMake, ngMakeText;
};
// A skill's icon source at `side` pixels (its aegis name in small letters), its width out; the emblem's at `side`; a
// controller button's glyph `h` high, its width out
using Icon = std::function<std::string(const std::string& aegis, int side, float& w)>;
using Emblem = std::function<std::string(int side)>;
using Glyph = std::function<std::string(int button, float h, float& w)>;

const char* rml();
void bind(Rml::DataModelConstructor& c, Model& m);
void fill(Model& m, const Frame& f, const Icon& icon, const Emblem& emblem, const Glyph& glyph);
bool refit(Rml::ElementDocument* doc, guildcard::Layout& L);

}  // namespace uagfx::uikit::guild
