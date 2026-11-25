#include "core/guild_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/client_libs.hpp"
#include "core/item_window.hpp"
#include "core/pack_source.hpp"

namespace uagfx::guildcard {
namespace {

namespace gs = uagfx::guildskills;

constexpr uint32_t kTabRights[kTabs] = {0, 0x01, 0x02, 0, 0x10, 0x80};  // FUN_0049c4d0's
// At scale 1 the head 46, the tabs 32 and 8 under them, the body, 8, the foot 40 with its line, the edges (RmlUi: 781)
constexpr float kChromeH = 46 + 32 + 8 + 8 + 41 + 4;
const char* const kDot = " \xC2\xB7 ";     // ·
const char* const kDash = "\xE2\x80\x94";  // —

std::string digits(long long v) {  // 1 240 520
    const std::string s = std::to_string(v < 0 ? -v : v);
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (i && (s.size() - i) % 3 == 0) out += ' ';
        out += s[i];
    }
    return v < 0 ? "-" + out : out;
}

const guildtabs::Member* listed(const guildtabs::Data& d, const Member& m) {  // its 0x0154 record
    for (const guildtabs::Member& x : d.members)
        if ((m.character && x.character == m.character) || (!m.character && !m.name.empty() && x.name == m.name))
            return &x;
    return nullptr;
}

const guildtabs::Position* positionOf(const guildtabs::Data& d, int id) {
    for (const guildtabs::Position& p : d.positions)
        if (p.id == id) return &p;
    return nullptr;
}

std::string rightsOf(const guildtabs::Position& p, const Texts& t) {
    std::string r;
    if (p.invite) r = t.invite;
    if (p.expel) r += (r.empty() ? "" : kDot) + t.expelRight;
    return r.empty() ? t.noRights : r;
}

unsigned long long givenAll(const guildtabs::Data& d) {
    unsigned long long all = 0;
    for (const guildtabs::Member& x : d.members) all += x.exp;
    return all;
}

std::string percent(unsigned long long part, unsigned long long all) {
    if (!all) return "0 %";
    const unsigned long long p = (part * 100 + all / 2) / all;
    return (p == 0 && part > 0 ? std::string("<1") : std::to_string(p)) + " %";
}

std::string lowerName(const char* aegis) { return itemwin::lowered(aegis); }

const Member* chosenMember(const std::vector<Member>& members, const std::vector<size_t>& order, const Show& sh) {
    for (size_t i : order)
        if (sh.member && members[i].character == sh.member) return &members[i];
    return order.empty() ? nullptr : &members[order.front()];
}

std::string kindOf(const gs::Row& r, const Texts& t) {
    if (r.kind == gs::Kind::Active) return t.kinds[2] + kDot + fmt(t.sp, {"1"});
    return t.kinds[r.kind == gs::Kind::Aura ? 1 : 0];
}

}  // namespace

std::string fmt(std::string pattern, const std::vector<std::string>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
        const std::string key = "{" + std::to_string(i + 1) + "}";
        for (size_t at = pattern.find(key); at != std::string::npos; at = pattern.find(key, at + values[i].size()))
            pattern.replace(at, key.size(), values[i]);
    }
    return pattern;
}

bool gameDrawn(int tab, bool editing) {
    return editing && (tab == int(Tab::Positions) || tab == int(Tab::Members));
}

bool allowed(int tab, uint32_t rights) {
    if (tab < 0 || tab >= kTabs) return false;
    return kTabRights[tab] == 0 || (rights & kTabRights[tab]) != 0;
}

int castles(const std::string& territory) {
    static const char* const kWords[] = {
        "one",       "two",       "three",     "four",     "five",       "six",        "seven",       "eight",
        "nine",      "ten",       "eleven",    "twelve",   "thirteen",   "fourteen",   "fifteen",     "sixteen",
        "seventeen", "eighteen",  "nineteen",  "twenty",   "twenty one", "twenty two", "twenty three"};
    std::string w;
    for (char c : territory) w += char(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
    while (!w.empty() && w.back() == ' ') w.pop_back();
    if (w == "none taken") return 0;
    if (w == "total domination") return 24;
    for (int i = 0; i < 23; ++i)
        if (w == std::string(kWords[i]) + (i ? " castles" : " castle")) return i + 1;
    return -1;
}

Layout layout(int targetW, int targetH, float scale) {
    Layout L;
    L.s = scale > 0 ? scale : 1.0f;
    L.w = int(std::ceil(kWide * L.s));
    L.h = int(std::ceil((kBodyH + kChromeH) * L.s));
    L.glass = panel::Box{0, 0, float(L.w), float(L.h)};
    L.x = std::clamp(int(float(targetW) * 0.56f), 0, std::max(0, targetW - L.w));
    L.y = std::clamp(int(float(targetH) * 0.12f), 0, std::max(0, targetH - L.h));
    return L;
}

float fitScale(int targetW, int targetH, float scale) {
    if (!(scale > 0) || targetW <= 0 || targetH <= 0) return scale;
    constexpr float kMargin = 0.96f;
    const float most = std::min(float(targetH) * kMargin / (kBodyH + kChromeH), float(targetW) * kMargin / kWide);
    return std::min(scale, most);
}

float thumbTop(int first, int total, int rows, float thumbH) {
    const int most = total - rows;
    if (most <= 0) return 0.0f;
    return (100.0f - thumbH) * float(std::clamp(first, 0, most)) / float(most);
}

int firstAt(float frac, int total, int rows) {
    const int most = total - rows;
    if (most <= 0) return 0;
    return std::clamp(int(std::lround(std::clamp(frac, 0.0f, 1.0f) * float(most))), 0, most);
}

Hit hitTest(const Layout& L, float x, float y) {
    const HitBox* best = nullptr;
    for (const HitBox& b : L.hits)
        if (b.box.contains(x, y) && (!best || b.box.w * b.box.h < best->box.w * best->box.h)) best = &b;
    if (best) return Hit{best->part, best->index};
    return L.glass.contains(x, y) ? Hit{Part::Body, -1} : Hit{};
}

// ---- «Навички»
SkillsView skillsView(const std::vector<gs::Row>& rows, const Show& sh, const Texts& t, bool master) {
    SkillsView v;
    const gs::Row* chosen = nullptr;
    int lastGroup = -1;
    for (const gs::Row& r : rows) {
        const gs::Def* d = gs::def(r.id);
        if (!d) continue;
        const int i = gs::indexOf(r.id);
        SkillRowV row;
        row.id = r.id;
        if (int(r.group) != lastGroup) {
            lastGroup = int(r.group);
            row.group = t.groups[lastGroup];
            row.groupMore = t.groupsMore[lastGroup];
        }
        row.aegis = lowerName(d->aegis);
        row.name = d->name;
        row.active = r.kind == gs::Kind::Active;
        row.kind = kindOf(r, t);
        row.locked = r.locked;
        row.learned = r.level > 0;
        if (r.locked) {
            std::string names;
            for (const gs::Need& n : r.missing) {
                const gs::Def* nd = gs::def(n.id);
                names += (names.empty() ? "" : ", ") + std::string(nd ? nd->name : "?") + ' ' + std::to_string(n.level);
            }
            row.info = fmt(t.need, {names});
            row.kind.clear();
        } else if (row.active && r.coolMs > 0) {
            row.info = fmt(t.coolRow, {gs::clock(r.coolMs)});
        } else {
            row.info = i >= 0 && i < 14 ? t.lines[i] : "";
        }
        row.level = std::to_string(r.level) + "/" + std::to_string(r.most);
        if (r.most <= 5)
            for (int n = 0; n < r.most; ++n) row.dots.push_back(n < r.level ? 1 : 0);
        row.focus = r.id == sh.skill;
        row.hot = sh.hover.part == Part::Skill && sh.hover.index == r.id;
        row.plus = r.plus;
        row.plusHot = sh.hover.part == Part::Plus && sh.hover.index == r.id;
        if (row.focus) chosen = &r;
        v.rows.push_back(std::move(row));
    }
    if (!chosen && !rows.empty()) {
        chosen = &rows.front();
        if (!v.rows.empty()) v.rows.front().focus = true;
    }
    if (!chosen) return v;
    const gs::Row& r = *chosen;
    const gs::Def* d = gs::def(r.id);
    const int i = gs::indexOf(r.id);
    SkillSideV& s = v.side;
    s.id = r.id;
    s.aegis = lowerName(d->aegis);
    s.name = d->name;
    s.active = r.kind == gs::Kind::Active;
    s.kind = kindOf(r, t) + kDot + fmt(t.levelOf, {std::to_string(r.level), std::to_string(r.most)});
    s.about = i >= 0 && i < 14 ? t.about[i] : "";
    if (!master && r.kind == gs::Kind::Aura && r.level > 0) s.you = fmt(t.youNear, {gs::auraBonus(r.id, r.level)});
    for (const gs::LevelLine& l : gs::levels(r.id, r.level, t.places))
        s.levels.push_back(LevelV{std::to_string(l.level), l.what, l.now});
    s.where = r.kind == gs::Kind::Aura ? t.whereAura : s.active ? t.whereWar : "";
    if (s.active) {
        s.cool = t.coolShared;
        s.ready = r.coolMs <= 0;
        s.wait = s.ready ? t.ready : fmt(t.wait, {gs::clock(r.coolMs)});
    }
    for (const gs::Need& n : d->needs) {
        const gs::Def* nd = gs::def(n.id);
        bool met = true;
        for (const gs::Need& m : r.missing) met = met && m.id != n.id;
        s.needs.push_back(NeedV{std::string(nd ? nd->name : "?") + ' ' + std::to_string(n.level), met});
    }
    s.use = r.usable;
    s.useOff = r.usable && r.coolMs > 0;
    s.useWord = t.use;
    s.useHot = sh.hover.part == Part::Use;
    s.raise = r.plus;
    s.raiseWord = fmt(t.raiseTo, {std::to_string(r.level + 1)});
    s.raiseHot = sh.hover.part == Part::Raise;
    if (!master) s.who = t.whoRaisesText;
    s.youLabel = t.youLabel;
    s.levelsLabel = t.levelsLabel;
    s.whereLabel = t.whereLabel;
    s.coolLabel = t.coolLabel;
    s.needsLabel = t.needsLabel;
    s.whoLabel = t.whoRaises;
    return v;
}

// ---- «Інформація»
InfoView infoView(const Info& g, const std::vector<Member>& members, const guildtabs::Data& d,
                  const std::vector<gs::Row>& skills, const Show& sh, const Texts& t) {
    InfoView v;
    const bool master = isMaster(sh.rights);
    int online = 0;
    for (const Member& m : members) online += m.online ? 1 : 0;
    v.name = g.name;
    v.sub = fmt(t.infoSub, {std::to_string(g.level), std::to_string(members.size()), std::to_string(g.most),
                            std::to_string(online)});
    if (g.nextExp > 0) {
        v.expShare = std::clamp(float(double(g.exp) / double(g.nextExp)), 0.0f, 1.0f);
        v.expLine = fmt(t.expLine, {digits(g.exp), digits(g.nextExp), std::to_string(int(v.expShare * 100)),
                                    std::to_string(g.level + 1)});
    }
    bool masterOnline = false;
    for (const Member& m : members) masterOnline = masterOnline || (m.name == g.master && m.online);
    v.facts.push_back(FactV{t.master, g.master.empty() ? t.none : g.master, masterOnline});
    v.facts.push_back(FactV{t.average, std::to_string(g.average), false});
    const int n = castles(g.territory);
    v.facts.push_back(
        FactV{t.castles, n > 0 ? std::to_string(n) : n == 0 || g.territory.empty() ? t.none : g.territory, false});
    for (const guildtabs::Member& x : d.members)  // the player's own position's tax, once both lists came
        if (!sh.me.empty() && x.name == sh.me)
            if (const guildtabs::Position* p = positionOf(d, x.position))
                v.facts.push_back(FactV{t.yourTax, fmt(t.yourTaxValue, {std::to_string(p->tax), p->name}), false});
    v.alliances = d.alliancesSeen;
    for (const guildtabs::Alliance& a : d.alliances) (a.enemy ? v.enemies : v.allies).push_back(a.name);
    v.alliesLabel = t.allies;
    v.enemiesLabel = t.enemies;
    v.subject = d.subject;
    v.body = d.body;
    v.noticeLabel = t.notice;
    v.giftsLabel = t.gifts;
    for (const gs::Row& r : skills) {
        const gs::Def* sd = gs::def(r.id);
        if (r.level <= 0 || !sd) continue;
        std::string what, from = std::string(sd->name) + kDot;
        if (r.kind == gs::Kind::Aura && !master) {  // the master never gets the auras (guild_aura's 16)
            what = gs::auraBonus(r.id, r.level);
            from += t.nearMaster;
        } else if (r.id == 10010 || r.id == 10011 || r.id == 10012) {
            for (const gs::LevelLine& l : gs::levels(r.id, r.level, ""))
                if (l.now) what = l.what;
            from += t.warOnly;
        }
        if (!what.empty()) v.gifts.push_back(GiftV{lowerName(sd->aegis), what, from});
    }
    std::vector<const Member*> on;
    for (const Member& m : members)
        if (m.online) on.push_back(&m);
    std::stable_sort(on.begin(), on.end(), [](const Member* a, const Member* b) { return a->level > b->level; });
    v.onlineHead = fmt(t.online, {std::to_string(on.size())});
    for (size_t i = 0; i < on.size() && i < 16; ++i)
        v.online.push_back(PersonV{on[i]->name, on[i]->job + kDot + std::to_string(on[i]->level)});
    v.offline = fmt(t.offline, {std::to_string(members.size() - on.size())});
    v.allWord = t.allMembers;
    v.none = t.none;
    return v;
}

// ---- «Учасники»
std::vector<size_t> memberOrder(const std::vector<Member>& members, const guildtabs::Data& d, const Show& sh) {
    std::vector<size_t> order;
    const std::string want = itemwin::lowered(sh.search);
    for (size_t i = 0; i < members.size(); ++i)
        if (want.empty() || itemwin::lowered(members[i].name).find(want) != std::string::npos) order.push_back(i);
    const auto pos = [&](size_t i) {
        const guildtabs::Member* x = listed(d, members[i]);
        return x && x->position >= 0 ? x->position : 1000;
    };
    const auto given = [&](size_t i) {
        const guildtabs::Member* x = listed(d, members[i]);
        return x ? (long long)x->exp : -1LL;
    };
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const Member &ma = members[a], &mb = members[b];
        switch (sh.sort) {
            case Sort::Level:
                if (ma.level != mb.level) return ma.level > mb.level;
                break;
            case Sort::Position:
                if (pos(a) != pos(b)) return pos(a) < pos(b);
                if (ma.level != mb.level) return ma.level > mb.level;
                break;
            case Sort::Given:
                if (given(a) != given(b)) return given(a) > given(b);
                break;
            case Sort::Online:
                if (ma.online != mb.online) return ma.online;
                if (pos(a) != pos(b)) return pos(a) < pos(b);
                if (ma.level != mb.level) return ma.level > mb.level;
                break;
        }
        return ma.name < mb.name;
    });
    return order;
}

MembersView membersView(const Info& g, const std::vector<Member>& members, const guildtabs::Data& d, const Show& sh,
                        const Texts& t, bool master, uint32_t acts) {
    MembersView v;
    const std::vector<size_t> order = memberOrder(members, d, sh);
    v.total = int(order.size());
    v.first = sh.first[1];
    v.count = fmt(t.count, {std::to_string(members.size()), std::to_string(g.most)});
    v.searchWord = sh.search;
    v.placeholder = t.search;
    v.positionLabel = t.position;
    v.givenLabel = t.given;
    v.rightsNote = master ? t.rightsNote : "";
    const std::string arrow = " \xE2\x86\x93";  // ↓
    const std::string* names[5] = {&t.colName, &t.colJob, &t.colLevel, &t.colPosition, &t.colGiven};
    const Sort sorts[5] = {Sort::Online, Sort::Online, Sort::Level, Sort::Position, Sort::Given};
    for (int i = 0; i < 5; ++i) {
        const bool on = i != 1 && sorts[i] == sh.sort;
        v.heads.push_back(HeadV{*names[i] + (on ? arrow : ""), on, sh.hover.part == Part::Head && sh.hover.index == i});
    }
    const unsigned long long all = givenAll(d);
    const Member* chosen = chosenMember(members, order, sh);
    const int first = std::clamp(sh.first[int(Tab::Members)], 0, std::max(0, int(order.size()) - kMemberRows));
    for (int n = first; n < int(order.size()) && n < first + kMemberRows; ++n) {
        const Member& m = members[order[size_t(n)]];
        const guildtabs::Member* x = listed(d, m);
        const guildtabs::Position* p = x ? positionOf(d, x->position) : nullptr;
        MemberRowV r;
        r.character = m.character;
        r.name = m.name;
        r.job = m.job;
        r.level = std::to_string(m.level);
        r.position = !m.title.empty() ? m.title : p ? p->name : kDash;
        r.given = x && d.membersSeen ? percent(x->exp, all) : kDash;
        r.online = m.online;
        r.focus = &m == chosen;
        r.hot = sh.hover.part == Part::Member && sh.hover.index == int(m.character);
        r.me = !sh.me.empty() && m.name == sh.me;
        v.rows.push_back(std::move(r));
    }
    if (!chosen) return v;
    CardV& c = v.card;
    c.shown = true;
    c.name = chosen->name;
    c.online = chosen->online;
    c.master = master;
    c.sub = chosen->job + kDot + t.colLevel + ' ' + std::to_string(chosen->level) + kDot +
            (chosen->online ? t.inGame : t.away);
    const guildtabs::Member* x = listed(d, *chosen);
    const guildtabs::Position* p = x ? positionOf(d, x->position) : nullptr;
    c.position = p ? p->name + " \xE2\x80\x94 " + rightsOf(*p, t) + kDot + std::to_string(p->tax) + " %"
                   : !chosen->title.empty() ? chosen->title : kDash;
    if (x && d.membersSeen) {
        int rank = 1;
        for (const guildtabs::Member& o : d.members) rank += o.exp > x->exp ? 1 : 0;
        c.given = fmt(t.givenValue, {percent(x->exp, all), std::to_string(rank), std::to_string(d.members.size())});
    } else {
        c.given = kDash;
    }
    if (sh.me.empty() || chosen->name != sh.me) {
        const std::string words[kActs] = {t.whisper, t.party, t.friendly,
                                          fmt(t.setPosition, {p ? p->name : chosen->title}), t.expel};
        for (int a = 0; a < kActs; ++a)
            if ((acts >> a & 1) && (a < int(Act::Position) || master)) c.actions.push_back({a, words[a]});
    }
    if (sh.hover.part == Part::Action) c.hotAction = sh.hover.index;
    return v;
}

// ---- «Посади»
PositionsView positionsView(const guildtabs::Data& d, const Show& sh, const Texts& t) {
    PositionsView v;
    for (const std::string* h : {&t.colPosition, &t.colRights, &t.colTax, &t.colPeople}) v.heads.push_back(HeadV{*h});
    std::vector<int> people(d.positions.size(), 0);
    for (const guildtabs::Member& x : d.members)
        for (size_t i = 0; i < d.positions.size(); ++i) people[i] += d.positions[i].id == x.position ? 1 : 0;
    int folded = 0;
    const guildtabs::Position* chosen = nullptr;
    for (size_t i = 0; i < d.positions.size(); ++i) {
        const guildtabs::Position& p = d.positions[i];
        if (d.membersSeen && people[i] == 0) {
            ++folded;
            continue;
        }
        PositionRowV r;
        r.id = p.id;
        r.name = p.name;
        if (p.invite) r.rights.push_back(t.invite);
        if (p.expel) r.rights.push_back(t.expelRight);
        r.tax = std::to_string(p.tax) + " %";
        r.people = d.membersSeen ? std::to_string(people[i]) : kDash;
        r.focus = p.id == sh.position;
        r.hot = sh.hover.part == Part::Position && sh.hover.index == p.id;
        if (r.focus) chosen = &p;
        v.rows.push_back(std::move(r));
    }
    if (!chosen && !v.rows.empty()) {
        v.rows.front().focus = true;
        chosen = positionOf(d, v.rows.front().id);
    }
    if (folded > 0) v.folded = fmt(t.folded, {std::to_string(folded)});
    if (chosen) {
        v.sideName = chosen->name;
        v.sideRights = rightsOf(*chosen, t);
        v.sideTax = std::to_string(chosen->tax) + " %";
        int n = 0;
        for (const guildtabs::Member& x : d.members) n += x.position == chosen->id ? 1 : 0;
        v.sidePeople = d.membersSeen ? fmt(t.peopleOn, {std::to_string(n)}) : "";
    }
    v.canEdit = isMaster(sh.rights);
    v.editWord = t.edit;
    v.editNote = t.editNote;
    v.editHot = sh.hover.part == Part::Edit;
    v.noRights = t.noRights;
    v.rightsLabel = t.colRights;
    v.taxLabel = t.colTax;
    return v;
}

// ---- «Вигнані»
ExpelledView expelledView(const guildtabs::Data& d, const Show& sh, const Texts& t) {
    ExpelledView v;
    v.head = t.expelledHead;
    v.accountLabel = t.account;
    v.reasonLabel = t.reason;
    v.nameLabel = t.colName;
    v.none = t.none;
    const int chosen = std::clamp(sh.expelled, 0, std::max(0, int(d.expelled.size()) - 1));
    const int first = std::clamp(sh.first[int(Tab::Expelled)], 0, std::max(0, int(d.expelled.size()) - kExpelledRows));
    for (int i = first; i < int(d.expelled.size()) && i < first + kExpelledRows; ++i) {
        const guildtabs::Expelled& e = d.expelled[size_t(i)];
        v.rows.push_back(
            ExpelledRowV{e.name, e.reason, i == chosen, sh.hover.part == Part::Expelled && sh.hover.index == i});
    }
    if (!d.expelled.empty()) {
        const guildtabs::Expelled& e = d.expelled[size_t(chosen)];
        v.sideName = e.name;
        v.sideAccount = e.account;
        v.sideReason = e.reason;
    }
    return v;
}

// ---- «Оголошення»
NoticeView noticeView(const guildtabs::Data& d, const Show& sh, const Texts& t, size_t subjectBytes,
                      size_t bodyBytes) {
    NoticeView v;
    v.editing = sh.editing;
    v.canEdit = isMaster(sh.rights);
    v.subject = sh.editing ? sh.subject : d.subject;
    v.body = sh.editing ? sh.body : d.body;
    v.empty = d.subject.empty() && d.body.empty();
    v.subjectCount = fmt(t.counter, {std::to_string(subjectBytes), std::to_string(kSubjectMost + 1)});
    v.bodyCount = fmt(t.counter, {std::to_string(bodyBytes), std::to_string(kBodyMost + 1)});
    v.fieldTitle = t.fieldTitle;
    v.fieldBody = t.fieldBody;
    v.hint = t.editHint;
    v.previewLabel = t.preview;
    v.previewNote = t.previewNote;
    v.saveWord = t.save;
    v.cancelWord = t.cancel;
    v.editWord = t.edit;
    v.label = t.notice;
    v.none = t.noticeEmpty;
    v.field = sh.field;
    v.caret = sh.caret;
    v.saveHot = sh.hover.part == Part::Save;
    v.cancelHot = sh.hover.part == Part::Cancel;
    v.editHot = sh.hover.part == Part::Edit;
    return v;
}

FootV footOf(const Show& sh, const Texts& t, int points, bool master) {
    FootV f;
    const auto hints = [](const std::string& words) {  // «a|b|c»
        std::vector<std::string> out;
        size_t from = 0;
        while (from <= words.size()) {
            const size_t at = words.find('|', from);
            const std::string one = words.substr(from, at == std::string::npos ? std::string::npos : at - from);
            if (!one.empty()) out.push_back(one);
            if (at == std::string::npos) break;
            from = at + 1;
        }
        return out;
    };
    constexpr int kA = 0, kB = 1, kX = 2, kY = 3, kLB = 4, kUp = 8;  // padglyph::Button's
    if (sh.tab == Tab::Skills) {
        f.left = master ? fmt(t.points, {std::to_string(points)}) : t.memberFoot;
        f.gold = master && points > 0;
        f.hints = hints(t.skillHints);
        if (!master && f.hints.size() > 1) f.hints.resize(1);  // a member uses and drags nothing
        f.pad = {{kLB, t.padTabs}, {kUp, t.padPick}};
        if (master) {
            f.pad.push_back({kA, t.padUse});
            f.pad.push_back({kY, t.padRaise});
            f.pad.push_back({kX, t.padAssign});
        }
    } else if (sh.tab == Tab::Members) {
        f.hints = hints(t.membersHints);
        f.pad = {{kLB, t.padTabs}, {kUp, t.padPick}, {kA, t.padActions}};
    } else {
        f.pad = {{kLB, t.padTabs}, {kUp, t.padPick}};
    }
    f.pad.push_back({kB, t.padClose});
    if (sh.pad) f.hints.clear();
    else f.pad.clear();
    return f;
}

std::vector<uint32_t> emblemPixels(const std::string& file, int side) {
    if (side <= 0 || file.size() < 2) return {};
    std::string bmp;
    if (file[0] == 'B' && file[1] == 'M') {
        bmp = file;
    } else {  // packed: an emblem's BMP is a few KB (24 x 24 at 24 bits 1782 bytes), at most 64 x 64 at 32
        std::vector<unsigned char> out(64 * 1024);
        unsigned long len = static_cast<unsigned long>(out.size());
        if (client_libs::zlibUncompress(out.data(), &len, reinterpret_cast<const unsigned char*>(file.data()),
                                        static_cast<unsigned long>(file.size())) != 0)
            return {};
        bmp.assign(reinterpret_cast<const char*>(out.data()), len);
    }
    std::vector<uint32_t> argb;
    int w = 0, h = 0;
    if (!pack::decodeBmp(reinterpret_cast<const uint8_t*>(bmp.data()), bmp.size(), argb, w, h) || w <= 0 || h <= 0 ||
        w > 256 || h > 256)
        return {};
    std::vector<uint32_t> px(size_t(side) * size_t(side), 0u);
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {
            const uint32_t c = argb[size_t(y * h / side) * size_t(w) + size_t(x * w / side)];
            const uint32_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
            px[size_t(y) * size_t(side) + size_t(x)] = r >= 0xF8 && g <= 0x08 && b >= 0xF8 ? 0u : (c | 0xFF000000u);
        }
    return px;
}

}  // namespace uagfx::guildcard
