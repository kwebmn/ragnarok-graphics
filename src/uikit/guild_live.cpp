#include "uikit/guild_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <string>

#include "uikit/hits.hpp"

namespace uagfx::uikit::guild {
namespace {

namespace gc = uagfx::guildcard;

const char* kRml = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw gdw" data-style-font-size="base + 'px'" data-style-background-color="ground"
  data-class-gwsmall="noGuild">
<div class="gwh" data-hit="title"><img class="gwe" data-if="emblem != ''" data-attr-src="emblem"
  data-style-width="emblemW + 'px'" data-style-height="emblemW + 'px'"/><span class="gwn">{{name}}</span><span
  class="gws">{{sub}}</span><span class="gwg"></span><span class="gwk">{{hotkey}}</span></div>
<div class="gwx" data-hit="close" data-class-hot="closeHot"><span class="cxa"></span><span class="cxb"></span></div>
<div class="gwng" data-if="noGuild"><span class="gwngt">{{ngHead}}</span><span
  class="gwngs">{{ngSub}}</span><div class="gwcds"><div class="gwcd"><span class="gwcdt">{{ngJoin}}</span><span
  class="gwcdx">{{ngJoinText}}</span></div><div class="gwcd"><span class="gwcdt">{{ngMake}}</span><span
  class="gwcdx">{{ngMakeText}}</span></div></div></div>
<div data-if="!noGuild"><div class="gwts"><div data-for="t, i : tabs"><div class="gwt" data-hit="tab"
  data-attr-data-index="i"
  data-class-on="t.on" data-class-hot="t.hot" data-class-off="t.off">{{t.name}}<span class="gwtn"
  data-if="t.count != ''">{{t.count}}</span><span class="gwtp"
  data-if="t.badge != ''">{{t.badge}}</span></div></div></div>
<div class="gwb"><div class="gwa" data-hit="area">
<div data-if="tab == 3"><div data-for="r : skills"><div class="gwgr" data-if="r.group != ''">{{r.group}}<span
  class="gwgm">{{r.groupMore}}</span></div><div class="gwsr" data-hit="skill" data-attr-data-index="r.id"
  data-class-focus="r.focus" data-class-hot="r.hot"><div class="gwic"><img data-if="r.icon != ''"
  data-attr-src="r.icon" data-style-width="r.iconW + 'px'" data-style-height="r.iconH + 'px'"
  data-style-opacity="r.op"/></div><div class="gwtx"><div class="gwnm" data-class-lock="r.locked">{{r.name}}</div><div
  class="gwin" data-class-need="r.locked"><span class="gwkd" data-if="r.kind != ''"
  data-class-act="r.active">{{r.kind}} &#xB7; </span>{{r.info}}</div></div><div class="gwds"><div
  data-for="d : r.dots"><span class="gwdt" data-class-lv="d == 1"></span></div></div><span class="gwlv"
  data-class-learned="r.learned">{{r.level}}</span><div class="gwpl"><span class="gwplus" data-if="r.plus"
  data-hit="plus" data-attr-data-index="r.id" data-class-hot="r.plusHot">+</span></div></div></div></div>
<div data-if="tab == 0"><div class="gwih"><img data-if="info.emblem != ''" data-attr-src="info.emblem"
  data-style-width="info.emblemW + 'px'" data-style-height="info.emblemW + 'px'"/><div class="gwit"><span
  class="gwinm">{{info.name}}</span><span class="gwisb">{{info.sub}}</span></div></div><div
  data-if="info.expLine != ''"><div class="gwtr"><div class="gwtf"
  data-style-width="info.expShare + '%'"></div></div><div
  class="gwcap">{{info.expLine}}</div></div><div class="gwfs"><div data-for="f : info.facts"><div class="gwfr"><span
  class="gwfk">{{f.a}}</span><span class="gwfv">{{f.b}}<span class="gwon" data-if="f.on"></span></span></div></div><div
  class="gwfr" data-if="info.alliances"><span class="gwfk">{{info.alliesLabel}}</span><div class="gwchs"><span
  class="gwfv" data-if="info.allies.size == 0">{{info.none}}</span><div data-for="a : info.allies"><span
  class="gwch">{{a}}</span></div></div></div><div class="gwfr" data-if="info.alliances"><span
  class="gwfk">{{info.enemiesLabel}}</span><div class="gwchs"><span class="gwfv"
  data-if="info.enemies.size == 0">{{info.none}}</span><div data-for="a : info.enemies"><span
  class="gwch gwfoe">{{a}}</span></div></div></div></div><div class="gwnt"
  data-if="info.subject != '' || info.body != ''"><span
  class="gwntl">{{info.noticeLabel}}</span><span class="gwnts">{{info.subject}}</span><span
  class="gwntb">{{info.body}}</span></div><div data-if="info.gifts.size > 0"><div
  class="gwl gwil">{{info.giftsLabel}}</div><div
  data-for="g : info.gifts"><div class="gwgf"><img data-attr-src="g.icon" data-style-width="g.iconW + 'px'"
  data-style-height="g.iconH + 'px'"/><span class="gwgw">{{g.what}}</span><span
  class="gwgo">{{g.from}}</span></div></div></div></div>
<div data-if="tab == 1"><div class="gwtl"><div class="gwse" data-hit="search" data-class-typing="members.typing"
  data-class-full="members.searched" data-class-hot="members.searchHot">{{members.shownSearch}}<span class="gwcaret"
  data-if="members.typing && members.caret"></span></div><span class="gwcn">{{members.count}}</span></div><div
  class="gwmh"><span class="gwc0"></span><span class="gwhc gwc1" data-hit="head" data-index="0"
  data-class-on="members.heads[0].on" data-class-hot="members.heads[0].hot">{{members.heads[0].a}}</span><span
  class="gwhc gwc2">{{members.heads[1].a}}</span><span class="gwhc gwc3" data-hit="head" data-index="2"
  data-class-on="members.heads[2].on" data-class-hot="members.heads[2].hot">{{members.heads[2].a}}</span><span
  class="gwhc gwc4" data-hit="head" data-index="3" data-class-on="members.heads[3].on"
  data-class-hot="members.heads[3].hot">{{members.heads[3].a}}</span><span class="gwhc gwc5" data-hit="head"
  data-index="4" data-class-on="members.heads[4].on"
  data-class-hot="members.heads[4].hot">{{members.heads[4].a}}</span></div><div
  data-for="r : members.rows"><div class="gwmr" data-hit="member" data-attr-data-index="r.ch" data-class-focus="r.focus"
  data-class-hot="r.hot" data-class-gone="!r.online" data-class-me="r.me"><span class="gwc0"><span class="gwdn"
  data-class-gwdf="!r.online"></span></span><span class="gwc1 gwmn">{{r.name}}</span><span
  class="gwc2">{{r.job}}</span><span class="gwc3">{{r.level}}</span><span class="gwc4">{{r.position}}</span><span
  class="gwc5">{{r.given}}</span></div></div><div class="gwsc" data-hit="mbar"
  data-if="members.scroll"><div class="gwsct" data-style-top="members.thumbTop + '%'"
  data-style-height="members.thumbH + '%'"></div></div></div>
<div data-if="tab == 2"><div class="gwmh"><span class="gwq1">{{positions.heads[0].a}}</span><span
  class="gwq2">{{positions.heads[1].a}}</span><span class="gwq3">{{positions.heads[2].a}}</span><span
  class="gwq4">{{positions.heads[3].a}}</span></div><div data-for="r : positions.rows"><div class="gwpr"
  data-hit="position" data-attr-data-index="r.id" data-class-focus="r.focus" data-class-hot="r.hot"><span
  class="gwq1 gwmn">{{r.name}}</span><div class="gwq2"><span class="gwrt" data-if="r.r1 != ''">{{r.r1}}</span><span
  class="gwrt" data-if="r.r2 != ''">{{r.r2}}</span><span class="gwrt gwrn"
  data-if="r.r1 == '' && r.r2 == ''">{{positions.noRights}}</span></div><span class="gwq3">{{r.tax}}</span><span
  class="gwq4">{{r.people}}</span></div></div><div class="gwfold"
  data-if="positions.folded != ''">{{positions.folded}}</div></div>
<div data-if="tab == 4"><div class="gwmh"><span class="gwx1">{{expelled.nameLabel}}</span><span
  class="gwx2">{{expelled.reasonLabel}}</span></div><div data-for="r : expelled.rows"><div class="gwer"
  data-hit="expelled" data-attr-data-index="r.i" data-class-focus="r.focus" data-class-hot="r.hot"><span
  class="gwx1 gwmn">{{r.name}}</span><span class="gwx2">{{r.reason}}</span></div></div><div class="gwfold"
  data-if="expelled.rows.size == 0">{{expelled.none}}</div></div>
<div data-if="tab == 5"><div data-if="notice.editing"><div
  class="gwfb"><span>{{notice.fieldTitle}}</span><span>{{notice.subjectCount}}</span></div><div
  class="gwfd" data-hit="field" data-index="0" data-class-typing="notice.field == 0">{{notice.subject}}<span
  class="gwcaret" data-if="notice.field == 0 && notice.caret"></span></div><div
  class="gwfb gwt8"><span>{{notice.fieldBody}}</span><span>{{notice.bodyCount}}</span></div><div
  class="gwfd gwtall" data-hit="field" data-index="1" data-class-typing="notice.field == 1">{{notice.body}}<span
  class="gwcaret" data-if="notice.field == 1 && notice.caret"></span></div><div
  class="gwfh2">{{notice.hint}}</div></div><div
  data-if="!notice.editing"><div class="gwnt"><span class="gwntl">{{notice.label}}</span><span
  class="gwnts">{{notice.subject}}</span><span class="gwntb">{{notice.body}}</span><span class="gwntb"
  data-if="notice.empty">{{notice.none}}</span></div></div></div>
</div><div class="gwp" data-hit="side">
<div data-if="tab == 3 && side.id > 0"><div class="gwsh"><img data-if="side.icon != ''" data-attr-src="side.icon"
  data-style-width="side.iconW + 'px'" data-style-height="side.iconH + 'px'"/><div class="gwst"><span
  class="gwsn">{{side.name}}</span><span class="gwsk" data-class-act="side.active">{{side.kind}}</span></div></div><div
  class="gwab">{{side.about}}</div><div data-if="side.you != ''"><div class="gwl">{{side.youLabel}}</div><div
  class="gwln gwgood">{{side.you}}</div></div><div data-if="side.levels.size > 0"><div
  class="gwl">{{side.levelsLabel}}</div><div
  data-for="l : side.levels"><div class="gwlr"
  data-class-now="l.on"><span>{{l.a}}</span><span>{{l.b}}</span></div></div></div><div
  data-if="side.where != ''"><div class="gwl">{{side.whereLabel}}</div><div class="gwln">{{side.where}}</div></div><div
  data-if="side.cool != ''"><div class="gwl">{{side.coolLabel}}</div><div class="gwln">{{side.cool}} <span
  class="gwgood" data-class-gwwait="!side.ready">{{side.wait}}</span></div></div><div
  data-if="side.needs.size > 0"><div class="gwl">{{side.needsLabel}}</div><div data-for="n : side.needs"><div
  class="gwnd"><span class="gwck" data-class-gwno="!n.on">{{n.a}}</span>{{n.b}}</div></div></div><div
  data-if="side.who != ''"><div class="gwl">{{side.whoLabel}}</div><div class="gwln">{{side.who}}</div></div><div
  class="gwbs" data-if="side.use || side.raise"><div class="gwbn gwmain" data-if="side.use" data-hit="use"
  data-class-gwdim="side.useOff" data-class-hot="side.useHot">{{side.useWord}}</div><div class="gwbn"
  data-if="side.raise"
  data-hit="raise" data-class-gwnext="side.use" data-class-hot="side.raiseHot">{{side.raiseWord}}</div></div></div>
<div data-if="tab == 0"><div class="gwl gwtop">{{info.onlineHead}}</div><div data-for="o : info.online"><div
  class="gwol"><span class="gwon gwon0"></span><span class="gwoln">{{o.a}}</span><span
  class="gwolj">{{o.b}}</span></div></div><div class="gwln gwmute">{{info.offline}}</div><div class="gwbs"><div
  class="gwbn" data-hit="allmembers" data-class-hot="info.allHot">{{info.allWord}}</div></div></div>
<div data-if="tab == 1 && members.card"><span class="gwsn">{{members.cardName}}</span><span
  class="gwsk">{{members.cardSub}}</span><div class="gwl">{{members.positionLabel}}</div><div
  class="gwln">{{members.cardPosition}}</div><div class="gwl">{{members.givenLabel}}</div><div
  class="gwln">{{members.cardGiven}}</div><div class="gwsep" data-if="members.actions.size > 0"></div><div
  data-for="a : members.actions"><div class="gwac" data-hit="action" data-attr-data-index="a.i" data-class-hot="a.hot"
  data-class-gwdanger="a.danger">{{a.name}}</div></div><div class="gwnote"
  data-if="members.rightsNote != ''">{{members.rightsNote}}</div></div>
<div data-if="tab == 2 && positions.sideName != ''"><span class="gwsn">{{positions.sideName}}</span><div
  class="gwl">{{positions.rightsLabel}}</div><div class="gwln">{{positions.sideRights}}</div><div
  class="gwl">{{positions.taxLabel}}</div><div class="gwln">{{positions.sideTax}}</div><div class="gwln gwt8"
  data-if="positions.sidePeople != ''">{{positions.sidePeople}}</div><div class="gwnote"
  data-if="positions.canEdit">{{positions.editNote}}</div><div class="gwbs" data-if="positions.canEdit"><div
  class="gwbn gwmain" data-hit="edit" data-class-hot="positions.editHot">{{positions.editWord}}</div></div></div>
<div data-if="tab == 4 && expelled.sideName != ''"><span class="gwsn">{{expelled.sideName}}</span><div
  class="gwl">{{expelled.accountLabel}}</div><div class="gwln">{{expelled.sideAccount}}</div><div
  class="gwl">{{expelled.reasonLabel}}</div><div class="gwln">{{expelled.sideReason}}</div></div>
<div data-if="tab == 5"><div class="gwl gwtop">{{notice.previewLabel}}</div><div class="gwnt gwnt0"><span
  class="gwntl">{{notice.label}}</span><span class="gwnts">{{notice.preSubject}}</span><span
  class="gwntb">{{notice.preBody}}</span></div><div class="gwln gwt8">{{notice.previewNote}}</div><div class="gwbs"
  data-if="notice.editing"><div class="gwbn gwmain" data-hit="save"
  data-class-hot="notice.saveHot">{{notice.saveWord}}</div><div
  class="gwbn gwnext" data-hit="cancel" data-class-hot="notice.cancelHot">{{notice.cancelWord}}</div></div><div
  class="gwbs"
  data-if="!notice.editing && notice.canEdit"><div class="gwbn gwmain" data-hit="edit"
  data-class-hot="notice.editHot">{{notice.editWord}}</div></div></div>
</div></div>
<div class="gwf" data-hit="foot"><span class="gwfl" data-class-gold="footGold">{{footLeft}}</span><div class="gwfh"><div
  data-for="h : hints"><span class="gwfi">{{h}}</span></div><div data-for="p : pad"><div class="gwpi"><img
  data-attr-src="p.glyph" data-style-width="p.w + 'px'"
  data-style-height="p.h + 'px'"/><span>{{p.words}}</span></div></div></div></div>
</div></body></rml>)RML";

}  // namespace

const char* rml() { return kRml; }

void bind(Rml::DataModelConstructor& c, Model& m) {
    c.RegisterArray<std::vector<int>>();
    c.RegisterArray<std::vector<Rml::String>>();
    if (auto e = c.RegisterStruct<TabV>()) {
        e.RegisterMember("name", &TabV::name);
        e.RegisterMember("count", &TabV::count);
        e.RegisterMember("badge", &TabV::badge);
        e.RegisterMember("on", &TabV::on);
        e.RegisterMember("hot", &TabV::hot);
        e.RegisterMember("off", &TabV::off);
    }
    c.RegisterArray<std::vector<TabV>>();
    if (auto e = c.RegisterStruct<PairV>()) {
        e.RegisterMember("a", &PairV::a);
        e.RegisterMember("b", &PairV::b);
        e.RegisterMember("on", &PairV::on);
        e.RegisterMember("hot", &PairV::hot);
    }
    c.RegisterArray<std::vector<PairV>>();
    if (auto e = c.RegisterStruct<SkillRowV>()) {
        e.RegisterMember("id", &SkillRowV::id);
        e.RegisterMember("group", &SkillRowV::group);
        e.RegisterMember("groupMore", &SkillRowV::groupMore);
        e.RegisterMember("icon", &SkillRowV::icon);
        e.RegisterMember("name", &SkillRowV::name);
        e.RegisterMember("kind", &SkillRowV::kind);
        e.RegisterMember("info", &SkillRowV::info);
        e.RegisterMember("level", &SkillRowV::level);
        e.RegisterMember("iconW", &SkillRowV::iconW);
        e.RegisterMember("iconH", &SkillRowV::iconH);
        e.RegisterMember("op", &SkillRowV::op);
        e.RegisterMember("active", &SkillRowV::active);
        e.RegisterMember("locked", &SkillRowV::locked);
        e.RegisterMember("learned", &SkillRowV::learned);
        e.RegisterMember("focus", &SkillRowV::focus);
        e.RegisterMember("hot", &SkillRowV::hot);
        e.RegisterMember("plus", &SkillRowV::plus);
        e.RegisterMember("plusHot", &SkillRowV::plusHot);
        e.RegisterMember("dots", &SkillRowV::dots);
    }
    c.RegisterArray<std::vector<SkillRowV>>();
    if (auto e = c.RegisterStruct<SideV>()) {
        e.RegisterMember("id", &SideV::id);
        e.RegisterMember("icon", &SideV::icon);
        e.RegisterMember("name", &SideV::name);
        e.RegisterMember("kind", &SideV::kind);
        e.RegisterMember("about", &SideV::about);
        e.RegisterMember("you", &SideV::you);
        e.RegisterMember("where", &SideV::where);
        e.RegisterMember("cool", &SideV::cool);
        e.RegisterMember("wait", &SideV::wait);
        e.RegisterMember("raiseWord", &SideV::raiseWord);
        e.RegisterMember("useWord", &SideV::useWord);
        e.RegisterMember("who", &SideV::who);
        e.RegisterMember("youLabel", &SideV::youLabel);
        e.RegisterMember("levelsLabel", &SideV::levelsLabel);
        e.RegisterMember("whereLabel", &SideV::whereLabel);
        e.RegisterMember("coolLabel", &SideV::coolLabel);
        e.RegisterMember("needsLabel", &SideV::needsLabel);
        e.RegisterMember("whoLabel", &SideV::whoLabel);
        e.RegisterMember("iconW", &SideV::iconW);
        e.RegisterMember("iconH", &SideV::iconH);
        e.RegisterMember("active", &SideV::active);
        e.RegisterMember("ready", &SideV::ready);
        e.RegisterMember("use", &SideV::use);
        e.RegisterMember("useOff", &SideV::useOff);
        e.RegisterMember("raise", &SideV::raise);
        e.RegisterMember("useHot", &SideV::useHot);
        e.RegisterMember("raiseHot", &SideV::raiseHot);
        e.RegisterMember("levels", &SideV::levels);
        e.RegisterMember("needs", &SideV::needs);
    }
    if (auto e = c.RegisterStruct<GiftV>()) {
        e.RegisterMember("icon", &GiftV::icon);
        e.RegisterMember("what", &GiftV::what);
        e.RegisterMember("from", &GiftV::from);
        e.RegisterMember("iconW", &GiftV::iconW);
        e.RegisterMember("iconH", &GiftV::iconH);
    }
    c.RegisterArray<std::vector<GiftV>>();
    if (auto e = c.RegisterStruct<InfoV>()) {
        e.RegisterMember("emblem", &InfoV::emblem);
        e.RegisterMember("name", &InfoV::name);
        e.RegisterMember("sub", &InfoV::sub);
        e.RegisterMember("expLine", &InfoV::expLine);
        e.RegisterMember("alliesLabel", &InfoV::alliesLabel);
        e.RegisterMember("enemiesLabel", &InfoV::enemiesLabel);
        e.RegisterMember("noticeLabel", &InfoV::noticeLabel);
        e.RegisterMember("giftsLabel", &InfoV::giftsLabel);
        e.RegisterMember("allWord", &InfoV::allWord);
        e.RegisterMember("none", &InfoV::none);
        e.RegisterMember("subject", &InfoV::subject);
        e.RegisterMember("body", &InfoV::body);
        e.RegisterMember("onlineHead", &InfoV::onlineHead);
        e.RegisterMember("offline", &InfoV::offline);
        e.RegisterMember("emblemW", &InfoV::emblemW);
        e.RegisterMember("expShare", &InfoV::expShare);
        e.RegisterMember("alliances", &InfoV::alliances);
        e.RegisterMember("allHot", &InfoV::allHot);
        e.RegisterMember("facts", &InfoV::facts);
        e.RegisterMember("online", &InfoV::online);
        e.RegisterMember("allies", &InfoV::allies);
        e.RegisterMember("enemies", &InfoV::enemies);
        e.RegisterMember("gifts", &InfoV::gifts);
    }
    if (auto e = c.RegisterStruct<MemberRowV>()) {
        e.RegisterMember("ch", &MemberRowV::ch);
        e.RegisterMember("name", &MemberRowV::name);
        e.RegisterMember("job", &MemberRowV::job);
        e.RegisterMember("level", &MemberRowV::level);
        e.RegisterMember("position", &MemberRowV::position);
        e.RegisterMember("given", &MemberRowV::given);
        e.RegisterMember("online", &MemberRowV::online);
        e.RegisterMember("focus", &MemberRowV::focus);
        e.RegisterMember("hot", &MemberRowV::hot);
        e.RegisterMember("me", &MemberRowV::me);
    }
    c.RegisterArray<std::vector<MemberRowV>>();
    if (auto e = c.RegisterStruct<ActionV>()) {
        e.RegisterMember("i", &ActionV::i);
        e.RegisterMember("name", &ActionV::name);
        e.RegisterMember("hot", &ActionV::hot);
        e.RegisterMember("danger", &ActionV::danger);
    }
    c.RegisterArray<std::vector<ActionV>>();
    if (auto e = c.RegisterStruct<MembersV>()) {
        e.RegisterMember("count", &MembersV::count);
        e.RegisterMember("shownSearch", &MembersV::shownSearch);
        e.RegisterMember("cardName", &MembersV::cardName);
        e.RegisterMember("cardSub", &MembersV::cardSub);
        e.RegisterMember("cardPosition", &MembersV::cardPosition);
        e.RegisterMember("cardGiven", &MembersV::cardGiven);
        e.RegisterMember("positionLabel", &MembersV::positionLabel);
        e.RegisterMember("givenLabel", &MembersV::givenLabel);
        e.RegisterMember("rightsNote", &MembersV::rightsNote);
        e.RegisterMember("typing", &MembersV::typing);
        e.RegisterMember("caret", &MembersV::caret);
        e.RegisterMember("searchHot", &MembersV::searchHot);
        e.RegisterMember("searched", &MembersV::searched);
        e.RegisterMember("card", &MembersV::card);
        e.RegisterMember("scroll", &MembersV::scroll);
        e.RegisterMember("thumbTop", &MembersV::thumbTop);
        e.RegisterMember("thumbH", &MembersV::thumbH);
        e.RegisterMember("heads", &MembersV::heads);
        e.RegisterMember("rows", &MembersV::rows);
        e.RegisterMember("actions", &MembersV::actions);
    }
    if (auto e = c.RegisterStruct<PositionRowV>()) {
        e.RegisterMember("id", &PositionRowV::id);
        e.RegisterMember("name", &PositionRowV::name);
        e.RegisterMember("tax", &PositionRowV::tax);
        e.RegisterMember("people", &PositionRowV::people);
        e.RegisterMember("r1", &PositionRowV::r1);
        e.RegisterMember("r2", &PositionRowV::r2);
        e.RegisterMember("focus", &PositionRowV::focus);
        e.RegisterMember("hot", &PositionRowV::hot);
    }
    c.RegisterArray<std::vector<PositionRowV>>();
    if (auto e = c.RegisterStruct<PositionsV>()) {
        e.RegisterMember("folded", &PositionsV::folded);
        e.RegisterMember("sideName", &PositionsV::sideName);
        e.RegisterMember("sideRights", &PositionsV::sideRights);
        e.RegisterMember("sideTax", &PositionsV::sideTax);
        e.RegisterMember("sidePeople", &PositionsV::sidePeople);
        e.RegisterMember("editWord", &PositionsV::editWord);
        e.RegisterMember("editNote", &PositionsV::editNote);
        e.RegisterMember("noRights", &PositionsV::noRights);
        e.RegisterMember("rightsLabel", &PositionsV::rightsLabel);
        e.RegisterMember("taxLabel", &PositionsV::taxLabel);
        e.RegisterMember("canEdit", &PositionsV::canEdit);
        e.RegisterMember("editHot", &PositionsV::editHot);
        e.RegisterMember("heads", &PositionsV::heads);
        e.RegisterMember("rows", &PositionsV::rows);
    }
    if (auto e = c.RegisterStruct<ExpelledRowV>()) {
        e.RegisterMember("i", &ExpelledRowV::i);
        e.RegisterMember("name", &ExpelledRowV::name);
        e.RegisterMember("reason", &ExpelledRowV::reason);
        e.RegisterMember("focus", &ExpelledRowV::focus);
        e.RegisterMember("hot", &ExpelledRowV::hot);
    }
    c.RegisterArray<std::vector<ExpelledRowV>>();
    if (auto e = c.RegisterStruct<ExpelledV>()) {
        e.RegisterMember("head", &ExpelledV::head);
        e.RegisterMember("nameLabel", &ExpelledV::nameLabel);
        e.RegisterMember("accountLabel", &ExpelledV::accountLabel);
        e.RegisterMember("reasonLabel", &ExpelledV::reasonLabel);
        e.RegisterMember("sideName", &ExpelledV::sideName);
        e.RegisterMember("sideAccount", &ExpelledV::sideAccount);
        e.RegisterMember("sideReason", &ExpelledV::sideReason);
        e.RegisterMember("none", &ExpelledV::none);
        e.RegisterMember("rows", &ExpelledV::rows);
    }
    if (auto e = c.RegisterStruct<NoticeV>()) {
        e.RegisterMember("subject", &NoticeV::subject);
        e.RegisterMember("body", &NoticeV::body);
        e.RegisterMember("preSubject", &NoticeV::preSubject);
        e.RegisterMember("preBody", &NoticeV::preBody);
        e.RegisterMember("subjectCount", &NoticeV::subjectCount);
        e.RegisterMember("bodyCount", &NoticeV::bodyCount);
        e.RegisterMember("fieldTitle", &NoticeV::fieldTitle);
        e.RegisterMember("fieldBody", &NoticeV::fieldBody);
        e.RegisterMember("hint", &NoticeV::hint);
        e.RegisterMember("previewLabel", &NoticeV::previewLabel);
        e.RegisterMember("previewNote", &NoticeV::previewNote);
        e.RegisterMember("saveWord", &NoticeV::saveWord);
        e.RegisterMember("cancelWord", &NoticeV::cancelWord);
        e.RegisterMember("editWord", &NoticeV::editWord);
        e.RegisterMember("label", &NoticeV::label);
        e.RegisterMember("none", &NoticeV::none);
        e.RegisterMember("editing", &NoticeV::editing);
        e.RegisterMember("canEdit", &NoticeV::canEdit);
        e.RegisterMember("empty", &NoticeV::empty);
        e.RegisterMember("caret", &NoticeV::caret);
        e.RegisterMember("saveHot", &NoticeV::saveHot);
        e.RegisterMember("cancelHot", &NoticeV::cancelHot);
        e.RegisterMember("editHot", &NoticeV::editHot);
        e.RegisterMember("field", &NoticeV::field);
    }
    if (auto e = c.RegisterStruct<PadV>()) {
        e.RegisterMember("glyph", &PadV::glyph);
        e.RegisterMember("words", &PadV::words);
        e.RegisterMember("w", &PadV::w);
        e.RegisterMember("h", &PadV::h);
    }
    c.RegisterArray<std::vector<PadV>>();
    c.Bind("base", &m.base);
    c.Bind("emblemW", &m.emblemW);
    c.Bind("ground", &m.ground);
    c.Bind("emblem", &m.emblem);
    c.Bind("name", &m.name);
    c.Bind("sub", &m.sub);
    c.Bind("hotkey", &m.hotkey);
    c.Bind("footLeft", &m.footLeft);
    c.Bind("closeHot", &m.closeHot);
    c.Bind("footGold", &m.footGold);
    c.Bind("tab", &m.tab);
    c.Bind("tabs", &m.tabs);
    c.Bind("skills", &m.skills);
    c.Bind("side", &m.side);
    c.Bind("info", &m.info);
    c.Bind("members", &m.members);
    c.Bind("positions", &m.positions);
    c.Bind("expelled", &m.expelled);
    c.Bind("notice", &m.notice);
    c.Bind("hints", &m.hints);
    c.Bind("pad", &m.pad);
    c.Bind("noGuild", &m.noGuild);
    c.Bind("ngHead", &m.ngHead);
    c.Bind("ngSub", &m.ngSub);
    c.Bind("ngJoin", &m.ngJoin);
    c.Bind("ngJoinText", &m.ngJoinText);
    c.Bind("ngMake", &m.ngMake);
    c.Bind("ngMakeText", &m.ngMakeText);
}

void fill(Model& m, const Frame& f, const Icon& icon, const Emblem& emblem, const Glyph& glyph) {
    const float s = f.scale > 0 ? f.scale : 1;
    const int small = std::max(8, int(std::lround(24 * s))), big = std::max(16, int(std::lround(48 * s)));
    m.base = 12 * s;
    m.emblemW = float(std::max(8, int(std::lround(28 * s))));
    m.emblem = emblem ? emblem(int(m.emblemW)) : "";
    m.name = f.name;
    m.sub = f.sub;
    m.hotkey = f.hotkey;
    m.closeHot = f.closeHot;
    m.tab = int(f.tab);
    m.tabs = f.tabs;
    m.noGuild = f.noGuild;
    m.ngHead = f.ngHead;
    m.ngSub = f.ngSub;
    m.ngJoin = f.ngJoin;
    m.ngJoinText = f.ngJoinText;
    m.ngMake = f.ngMake;
    m.ngMakeText = f.ngMakeText;
    const auto iconOf = [&](const std::string& aegis, int side, float& w, float& h) {
        w = h = 0;
        if (!icon || aegis.empty()) return std::string();
        float got = 0;
        std::string src = icon(aegis, side, got);
        if (src.empty()) return src;
        w = h = got > 0 ? got : float(side);
        return src;
    };
    m.skills.clear();
    for (const gc::SkillRowV& r : f.skills.rows) {
        SkillRowV v;
        v.id = r.id;
        v.group = r.group;
        v.groupMore = r.groupMore;
        v.icon = iconOf(r.aegis, small, v.iconW, v.iconH);
        v.op = r.locked ? 0.35f : r.learned ? 1.0f : 0.55f;
        v.name = r.name;
        v.kind = r.kind;
        v.info = r.info;
        v.level = r.level;
        v.active = r.active;
        v.locked = r.locked;
        v.learned = r.learned;
        v.focus = r.focus;
        v.hot = r.hot;
        v.plus = r.plus;
        v.plusHot = r.plusHot;
        v.dots = r.dots;
        m.skills.push_back(std::move(v));
    }
    const gc::SkillSideV& sd = f.skills.side;
    SideV& side = m.side;
    side = SideV{};
    side.id = sd.id;
    side.icon = iconOf(sd.aegis, big, side.iconW, side.iconH);
    side.name = sd.name;
    side.kind = sd.kind;
    side.about = sd.about;
    side.you = sd.you;
    side.where = sd.where;
    side.cool = sd.cool;
    side.wait = sd.wait;
    side.raiseWord = sd.raiseWord;
    side.useWord = sd.useWord;
    side.who = sd.who;
    side.youLabel = sd.youLabel;
    side.levelsLabel = sd.levelsLabel;
    side.whereLabel = sd.whereLabel;
    side.coolLabel = sd.coolLabel;
    side.needsLabel = sd.needsLabel;
    side.whoLabel = sd.whoLabel;
    side.active = sd.active;
    side.ready = sd.ready;
    side.use = sd.use;
    side.useOff = sd.useOff;
    side.raise = sd.raise;
    side.useHot = sd.useHot;
    side.raiseHot = sd.raiseHot;
    for (const gc::LevelV& l : sd.levels) side.levels.push_back(PairV{l.n, l.what, l.now, false});
    for (const gc::NeedV& n : sd.needs)
        side.needs.push_back(PairV{n.met ? "\xE2\x9C\x93" : "\xE2\x80\xA2", n.what, n.met, false});  // ✓ •
    const gc::InfoView& iv = f.info;
    InfoV& info = m.info;
    info = InfoV{};
    info.emblemW = float(big);
    info.emblem = emblem ? emblem(big) : "";
    info.name = iv.name;
    info.sub = iv.sub;
    info.expLine = iv.expLine;
    info.expShare = iv.expShare * 100;
    info.alliesLabel = iv.alliesLabel;
    info.enemiesLabel = iv.enemiesLabel;
    info.noticeLabel = iv.noticeLabel;
    info.giftsLabel = iv.giftsLabel;
    info.allWord = iv.allWord;
    info.none = iv.none;
    info.subject = iv.subject;
    info.body = iv.body;
    info.onlineHead = iv.onlineHead;
    info.offline = iv.offline;
    info.alliances = iv.alliances;
    info.allHot = f.allHot;
    for (const gc::FactV& x : iv.facts) info.facts.push_back(PairV{x.label, x.value, x.dot, false});
    for (const gc::PersonV& p : iv.online) info.online.push_back(PairV{p.name, p.job, true, false});
    for (const std::string& a : iv.allies) info.allies.push_back(a);
    for (const std::string& a : iv.enemies) info.enemies.push_back(a);
    for (const gc::GiftV& g : iv.gifts) {
        GiftV v;
        v.icon = iconOf(g.aegis, small, v.iconW, v.iconH);
        v.what = g.what;
        v.from = g.from;
        info.gifts.push_back(std::move(v));
    }
    const gc::MembersView& mv = f.members;
    MembersV& ms = m.members;
    ms = MembersV{};
    ms.count = mv.count;
    ms.searched = !mv.searchWord.empty();
    ms.shownSearch = ms.searched || f.typing ? mv.searchWord : mv.placeholder;
    ms.typing = f.typing;
    ms.caret = f.caret;
    ms.searchHot = f.searchHot;
    ms.positionLabel = mv.positionLabel;
    ms.givenLabel = mv.givenLabel;
    ms.rightsNote = mv.rightsNote;
    for (size_t i = 0; i < ms.heads.size(); ++i)  // the five there from the making
        ms.heads[i] = i < mv.heads.size() ? PairV{mv.heads[i].name, "", mv.heads[i].on, mv.heads[i].hot} : PairV{};
    for (const gc::MemberRowV& r : mv.rows) {
        MemberRowV v;
        v.ch = int(r.character);
        v.name = r.name;
        v.job = r.job;
        v.level = r.level;
        v.position = r.position;
        v.given = r.given;
        v.online = r.online;
        v.focus = r.focus;
        v.hot = r.hot;
        v.me = r.me;
        ms.rows.push_back(std::move(v));
    }
    ms.scroll = mv.total > gc::kMemberRows;
    if (ms.scroll) {
        ms.thumbH = std::max(100.0f * float(gc::kMemberRows) / float(mv.total), 8.0f);
        ms.thumbTop = gc::thumbTop(mv.first, mv.total, gc::kMemberRows, ms.thumbH);  // 3.0.23-beta.15: was 0
    }
    ms.card = mv.card.shown;
    ms.cardName = mv.card.name;
    ms.cardSub = mv.card.sub;
    ms.cardPosition = mv.card.position;
    ms.cardGiven = mv.card.given;
    for (const auto& [act, words] : mv.card.actions)
        ms.actions.push_back(ActionV{act, words, act == mv.card.hotAction, act == int(gc::Act::Expel)});
    const gc::PositionsView& pv = f.positions;
    PositionsV& ps = m.positions;
    ps = PositionsV{};
    for (size_t i = 0; i < ps.heads.size(); ++i)  // the four there from the making
        ps.heads[i] = i < pv.heads.size() ? PairV{pv.heads[i].name, "", false, false} : PairV{};
    for (const gc::PositionRowV& r : pv.rows) {
        PositionRowV v;
        v.id = r.id;
        v.name = r.name;
        v.tax = r.tax;
        v.people = r.people;
        if (!r.rights.empty()) v.r1 = r.rights[0];
        if (r.rights.size() > 1) v.r2 = r.rights[1];
        v.focus = r.focus;
        v.hot = r.hot;
        ps.rows.push_back(std::move(v));
    }
    ps.folded = pv.folded;
    ps.sideName = pv.sideName;
    ps.sideRights = pv.sideRights;
    ps.sideTax = pv.sideTax;
    ps.sidePeople = pv.sidePeople;
    ps.editWord = pv.editWord;
    ps.editNote = pv.editNote;
    ps.noRights = pv.noRights;
    ps.rightsLabel = pv.rightsLabel;
    ps.taxLabel = pv.taxLabel;
    ps.canEdit = pv.canEdit;
    ps.editHot = pv.editHot;
    const gc::ExpelledView& ev = f.expelled;
    ExpelledV& ex = m.expelled;
    ex = ExpelledV{};
    ex.head = ev.head;
    ex.nameLabel = ev.nameLabel;
    ex.accountLabel = ev.accountLabel;
    ex.reasonLabel = ev.reasonLabel;
    ex.sideName = ev.sideName;
    ex.sideAccount = ev.sideAccount;
    ex.sideReason = ev.sideReason;
    ex.none = ev.none;
    for (size_t i = 0; i < ev.rows.size(); ++i)
        ex.rows.push_back(ExpelledRowV{int(i), ev.rows[i].name, ev.rows[i].reason, ev.rows[i].focus, ev.rows[i].hot});
    const gc::NoticeView& nv = f.notice;
    NoticeV& no = m.notice;
    no = NoticeV{};
    no.subject = nv.subject;
    no.body = nv.body;
    no.preSubject = nv.subject;
    no.preBody = nv.body;
    no.subjectCount = nv.subjectCount;
    no.bodyCount = nv.bodyCount;
    no.fieldTitle = nv.fieldTitle;
    no.fieldBody = nv.fieldBody;
    no.hint = nv.hint;
    no.previewLabel = nv.previewLabel;
    no.previewNote = nv.previewNote;
    no.saveWord = nv.saveWord;
    no.cancelWord = nv.cancelWord;
    no.editWord = nv.editWord;
    no.label = nv.label;
    no.none = nv.none;
    no.editing = nv.editing;
    no.canEdit = nv.canEdit;
    no.empty = nv.empty && !nv.editing;
    no.caret = nv.caret;
    no.saveHot = nv.saveHot;
    no.cancelHot = nv.cancelHot;
    no.editHot = nv.editHot;
    no.field = nv.field;
    m.footLeft = f.foot.left;
    m.footGold = f.foot.gold;
    m.hints.clear();
    for (const std::string& h : f.foot.hints) m.hints.push_back(h);
    m.pad.clear();
    const float h = std::round(17 * s);
    for (const auto& [button, words] : f.foot.pad) {
        PadV v;
        v.h = h;
        v.glyph = glyph ? glyph(button, h, v.w) : "";
        v.words = words;
        m.pad.push_back(std::move(v));
    }
}

bool refit(Rml::ElementDocument* doc, gc::Layout& L) {
    if (!doc || !doc->IsVisible()) return false;
    const Rml::Vector2f size = doc->GetBox().GetSize(Rml::BoxArea::Border);
    if (size.x <= 0 || size.y <= 0) return false;
    static const std::pair<const char*, gc::Part> kParts[] = {
        {"title", gc::Part::Title},       {"close", gc::Part::Close},   {"tab", gc::Part::Tab},
        {"skill", gc::Part::Skill},       {"plus", gc::Part::Plus},     {"use", gc::Part::Use},
        {"raise", gc::Part::Raise},       {"member", gc::Part::Member}, {"head", gc::Part::Head},
        {"search", gc::Part::Search},     {"action", gc::Part::Action}, {"position", gc::Part::Position},
        {"edit", gc::Part::Edit},         {"expelled", gc::Part::Expelled}, {"field", gc::Part::Field},
        {"save", gc::Part::Save},         {"cancel", gc::Part::Cancel}, {"allmembers", gc::Part::AllMembers},
        {"mbar", gc::Part::MembersBar}};
    L.hits.clear();
    for (const auto& [name, part] : kParts)
        for (const Placed& p : placesOf(doc, name)) L.hits.push_back(gc::HitBox{part, p.index, p.box});
    L.glass = panel::Box{0, 0, size.x, size.y};
    L.w = int(std::ceil(size.x));
    L.h = int(std::ceil(size.y));
    return true;
}

}  // namespace uagfx::uikit::guild
