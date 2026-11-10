#include "uikit/companion_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>

#include "core/companion_skills.hpp"
#include "core/settings/strings.hpp"
#include "uikit/hits.hpp"

namespace uagfx::uikit::companion {
namespace {

namespace hc = uagfx::homuncard;
namespace mc = uagfx::merccard;

const char* kRml = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw cmp" data-style-background-color="ground">
<div class="head" data-hit="title"><span class="title">{{title}}</span></div>
<div class="x" data-hit="close" data-class-hot="closeHot">&#x2715;</div>
<div class="nrow"><span class="cn">{{name}}</span><span class="clv">{{level}}</span></div>
<div class="bars"><div class="b" data-for="b : bars"><div class="bl"><span class="k">{{b.label}}</span><span
  class="v">{{b.value}}</span></div><div class="track"><div data-attr-class="'fill ' + b.cls"
  data-style-width="b.fill + '%'"></div></div></div></div>
<div class="facts"><div class="f" data-for="f : facts"><span class="k">{{f.label}}</span><span
  data-attr-class="f.cls">{{f.value}}</span></div></div>
<div class="cstats"><div class="cst" data-for="st : stats"><span class="k">{{st.name}}</span><span
  class="v">{{st.value}}</span></div></div>
<div class="feed" data-if="feed != ''" data-hit="feed" data-class-hot="feedHot"
  data-class-focus="feedFocus">{{feed}}</div>
<div class="shd" data-hit="skillshead" data-class-focus="headFocus">{{skillsHead}}</div>
<div class="sks"><div class="sk" data-for="r, i : skills" data-hit="skill" data-attr-data-index="i"
  data-class-hot="r.hot" data-class-pad="r.pad"><span class="skn" data-class-dash="r.dash">{{r.name}}</span><span
  class="ar" data-hit="less" data-attr-data-index="i" data-class-off="!r.arrows"
  data-class-hot="r.lessHot">&#x2039;</span><span class="slv" data-hit="level" data-attr-data-index="i"
  data-class-gold="r.below">{{r.level}}</span><span class="ar" data-hit="more" data-attr-data-index="i"
  data-class-off="!r.arrows" data-class-hot="r.moreHot">&#x203A;</span><span class="plus" data-if="raise"
  data-hit="plus" data-attr-data-index="i" data-class-off="!r.plus" data-class-hot="r.plusHot">+</span></div></div>
<div class="rm" data-hit="remove" data-class-hot="removeHot">{{remove}}</div>
</body></rml>)RML";

const char* const kStatNames[8] = {"ATK", "MATK", "HIT", "CRIT", "DEF", "MDEF", "FLEE", "ASPD"};

std::string ofMost(long long now, long long most) { return std::to_string(now) + " / " + std::to_string(most); }
float share(long long now, long long most) {
    return most > 0 && now > 0 ? 100 * std::min(1.0f, float(now) / float(most)) : 0;
}

void common(Model& m, const std::string& name, int level, const std::string& levelForm) {
    m.name = name;
    std::string lv = levelForm;
    if (const size_t p = lv.find("{1}"); p != std::string::npos) lv.replace(p, 3, std::to_string(level));
    m.level = lv;
}

panel::Box one(Rml::ElementDocument* doc, const char* part) {
    const std::vector<Placed> p = placesOf(doc, part);
    return p.empty() ? panel::Box{} : p.front().box;
}

// The skills' boxes by row: every vector as long as `rows` (its core one's), w 0 where nothing is shown
void rowsOf(Rml::ElementDocument* doc, const char* part, std::vector<panel::Box>& to) {
    for (panel::Box& b : to) b = panel::Box{};
    for (const Placed& p : placesOf(doc, part))
        if (p.index >= 0 && p.index < int(to.size())) to[size_t(p.index)] = p.box;
}

bool sized(Rml::ElementDocument* doc, panel::Box& glass, int& w, int& h) {
    if (!doc || !doc->IsVisible()) return false;
    const Rml::Vector2f size = doc->GetBox().GetSize(Rml::BoxArea::Border);
    if (size.x <= 0 || size.y <= 0) return false;
    glass = panel::Box{0, 0, size.x, size.y};
    w = int(std::ceil(size.x));
    h = int(std::ceil(size.y));
    return true;
}

}  // namespace

const char* rml() { return kRml; }

void bind(Rml::DataModelConstructor& c, Model& m) {
    if (auto e = c.RegisterStruct<BarV>()) {
        e.RegisterMember("label", &BarV::label);
        e.RegisterMember("value", &BarV::value);
        e.RegisterMember("cls", &BarV::cls);
        e.RegisterMember("fill", &BarV::fill);
    }
    if (auto e = c.RegisterStruct<FactV>()) {
        e.RegisterMember("label", &FactV::label);
        e.RegisterMember("value", &FactV::value);
        e.RegisterMember("cls", &FactV::cls);
    }
    if (auto e = c.RegisterStruct<StatV>()) {
        e.RegisterMember("name", &StatV::name);
        e.RegisterMember("value", &StatV::value);
    }
    if (auto e = c.RegisterStruct<SkillV>()) {
        e.RegisterMember("name", &SkillV::name);
        e.RegisterMember("level", &SkillV::level);
        e.RegisterMember("arrows", &SkillV::arrows);
        e.RegisterMember("plus", &SkillV::plus);
        e.RegisterMember("below", &SkillV::below);
        e.RegisterMember("hot", &SkillV::hot);
        e.RegisterMember("pad", &SkillV::pad);
        e.RegisterMember("lessHot", &SkillV::lessHot);
        e.RegisterMember("moreHot", &SkillV::moreHot);
        e.RegisterMember("plusHot", &SkillV::plusHot);
        e.RegisterMember("dash", &SkillV::dash);
    }
    c.RegisterArray<std::vector<BarV>>();
    c.RegisterArray<std::vector<FactV>>();
    c.RegisterArray<std::vector<StatV>>();
    c.RegisterArray<std::vector<SkillV>>();
    c.Bind("ground", &m.ground);
    c.Bind("title", &m.title);
    c.Bind("name", &m.name);
    c.Bind("level", &m.level);
    c.Bind("feed", &m.feed);
    c.Bind("skillsHead", &m.skillsHead);
    c.Bind("remove", &m.remove);
    c.Bind("closeHot", &m.closeHot);
    c.Bind("feedHot", &m.feedHot);
    c.Bind("feedFocus", &m.feedFocus);
    c.Bind("removeHot", &m.removeHot);
    c.Bind("raise", &m.raise);
    c.Bind("headFocus", &m.headFocus);
    c.Bind("bars", &m.bars);
    c.Bind("facts", &m.facts);
    c.Bind("stats", &m.stats);
    c.Bind("skills", &m.skills);
}

void fillHomun(Model& m, const hc::Info& i, const hc::Show& sh, const hc::Texts& t) {
    m.title = t.title;
    common(m, i.name, i.level, t.level);
    m.closeHot = sh.hover == int(hc::Button::Close);
    m.bars = {BarV{"HP", ofMost(i.hp, i.maxHp), "bhp", share(i.hp, i.maxHp)},
              BarV{"SP", ofMost(i.sp, i.maxSp), "bsp", share(i.sp, i.maxSp)},
              BarV{"EXP", ofMost(i.exp, i.maxExp), "bexp", share(i.exp, i.maxExp)},
              BarV{t.hunger, ofMost(i.hunger, 100), "bfull", share(i.hunger, 100)}};
    m.facts = {FactV{t.intimacy, t.intimacyStates[hc::intimacyState(i.intimacy)], "fv love"}};
    m.stats.clear();
    for (int k = 0; k < 8; ++k)
        m.stats.push_back(StatV{kStatNames[k], std::to_string(k < 7 ? i.stats[k] : hc::aspd(i.aspd))});
    m.feed = t.feed;
    m.feedHot = sh.hover == int(hc::Button::Feed) || sh.focus == int(hc::Button::Feed);
    m.feedFocus = sh.focus == int(hc::Button::Feed);
    m.skillsHead = settings::fmt(t.skillsHead, {std::to_string(i.points)});
    m.headFocus = false;
    m.raise = !i.skills.empty();
    const size_t had = m.skills.size();
    m.skills.clear();
    for (size_t k = 0; k < i.skills.size(); ++k) {
        const hc::SkillRow& sk = i.skills[k];
        SkillV v;
        v.name = sk.name;
        const int use = uagfx::companion::useOf(sk.use, sk.level);
        v.below = use < sk.level;
        v.level = settings::fmt(t.skillLevel, {v.below ? std::to_string(use) + "/" + std::to_string(sk.level)
                                                       : std::to_string(sk.level)});
        v.arrows = sk.level > 1;
        v.plus = sk.up && i.points > 0;
        v.hot = sh.hoverSkill == int(k);
        v.pad = sh.padRow == int(k);
        v.lessHot = sh.hoverLess == int(k);
        v.moreHot = sh.hoverMore == int(k);
        v.plusHot = sh.hoverPlus == int(k);
        m.skills.push_back(v);
    }
    if (m.skills.empty()) m.skills.push_back(SkillV{"\xE2\x80\x94", "", false, false, false, false, false, false,
                                                    false, false, true});  // —
    if (m.skills.size() < had) m.skills.resize(had);  // RmlUi's lists never shrink under it
    m.remove = t.remove;
    m.removeHot = sh.hover == int(hc::Button::Delete);
}

void fillMerc(Model& m, const mc::Info& i, const mc::Show& sh, const mc::Texts& t) {
    m.title = t.kinds[int(i.kind)];
    common(m, i.name, i.level, t.level);
    m.closeHot = sh.hover == int(mc::Button::Close);
    m.bars = {BarV{"HP", ofMost(i.hp, i.maxHp), "bhp", share(i.hp, i.maxHp)},
              BarV{"SP", ofMost(i.sp, i.maxSp), "bsp", share(i.sp, i.maxSp)}};
    const std::string facts[4] = {i.ends, std::to_string(i.loyalty), std::to_string(i.summons),
                                  std::to_string(i.kills)};
    m.facts.clear();
    for (int k = 0; k < 4; ++k) m.facts.push_back(FactV{t.facts[k], facts[k], k == 0 ? "fv gold" : "fv"});
    m.stats.clear();
    for (int k = 0; k < 8; ++k)
        m.stats.push_back(StatV{kStatNames[k], std::to_string(k < 7 ? i.stats[k] : (2000 - i.aspd) / 10)});
    m.feed.clear();
    m.skillsHead = t.skills;
    m.headFocus = sh.focus;
    m.raise = false;
    const size_t had = m.skills.size();
    m.skills.clear();
    for (size_t k = 0; k < i.skills.size(); ++k) {
        const mc::SkillRow& sk = i.skills[k];
        SkillV v;
        v.name = sk.name;
        const int use = sk.usable ? uagfx::companion::useOf(sk.use, sk.level) : sk.level;
        v.below = use < sk.level;
        std::string lv = t.skillLevel;
        if (const size_t p = lv.find("{1}"); p != std::string::npos)
            lv.replace(p, 3, v.below ? std::to_string(use) + "/" + std::to_string(sk.level)
                                     : std::to_string(sk.level));
        v.level = lv;
        v.arrows = sk.usable && sk.level > 1;
        v.hot = sh.hoverSkill == int(k);
        v.pad = sh.padRow == int(k);
        v.lessHot = sh.hoverLess == int(k);
        v.moreHot = sh.hoverMore == int(k);
        m.skills.push_back(v);
    }
    if (m.skills.empty()) m.skills.push_back(SkillV{"\xE2\x80\x94", "", false, false, false, false, false, false,
                                                    false, false, true});
    if (m.skills.size() < had) m.skills.resize(had);
    m.remove = t.dismiss;
    m.removeHot = sh.hover == int(mc::Button::Dismiss);
}

bool refitHomun(Rml::ElementDocument* doc, hc::Layout& L) {
    if (!sized(doc, L.glass, L.w, L.h)) return false;
    L.title = one(doc, "title");
    L.close = one(doc, "close");
    L.buttons[0] = one(doc, "feed");
    L.skillsHead = one(doc, "skillshead");
    L.remove = one(doc, "remove");
    rowsOf(doc, "skill", L.skillRows);
    rowsOf(doc, "plus", L.skillPlus);
    rowsOf(doc, "less", L.skillLess);
    rowsOf(doc, "more", L.skillMore);
    rowsOf(doc, "level", L.skillLevel);
    return true;
}

bool refitMerc(Rml::ElementDocument* doc, mc::Layout& L) {
    if (!sized(doc, L.glass, L.w, L.h)) return false;
    L.title = one(doc, "title");
    L.close = one(doc, "close");
    L.skills = one(doc, "skillshead");
    L.dismiss = one(doc, "remove");
    rowsOf(doc, "skill", L.skillRows);
    rowsOf(doc, "less", L.skillLess);
    rowsOf(doc, "more", L.skillMore);
    rowsOf(doc, "level", L.skillLevel);
    return true;
}

}  // namespace uagfx::uikit::companion
