#include "win/target_card.hpp"
#include "win/game_memory.hpp"
#include "win/own_windows.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

#include "core/ui_glide.hpp"
#include "core/bar_hint.hpp"
#include "core/reference_pages.hpp"
#include "core/settings/strings.hpp"
#include "core/target_card.hpp"
#include "core/ui_motion.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "pad/win/runtime.hpp"
#include "win/game_camera.hpp"
#include "win/hotbar.hpp"
#include "win/hud_edit.hpp"
#include "win/log.hpp"
#include "win/mob_hp.hpp"
#include "win/monster_picture.hpp"
#include "win/pack_pictures.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/reference_window.hpp"
#include "win/ui_cost.hpp"
#include "win/uaui.hpp"
#include "win/window_input.hpp"
#include "core/pad_glyphs.hpp"
#include "core/ui_icons.hpp"

namespace uagfx::win::targetpanel {
namespace {

namespace tc = uagfx::targetcard;

constexpr uint32_t kOwnAccount = 0x771F7C;
// The world's actors (FUN_004ebd10: a list at world + 0x18, each node's actor at +8, its id at +0x208) and an actor's
// class (FUN_0054aee0: +0x20C); the game mode's world at +0xC4
constexpr uint32_t kWorld = 0xC4, kActors = 0x18, kActorId = 0x208, kActorClass = 0x20C;
// The game's windows (g_windowMgr's list at +0x38, each node's window at +8): shown (+0x38), x, y, w, h (+0x14..+0x20)
constexpr uint32_t kWindowMgr = 0x727ED8, kWindows = 0x38, kShown = 0x38, kRect = 0x14;
constexpr ULONGLONG kForgetMs = 12000;  // no hit with it this long: no target
constexpr size_t kMostNodes = 4096;

struct State {
    uint32_t target = 0;  // the actor's id
    uint32_t dismissed = 0;  // 3.0.18-beta.120 (the concept «Ігровий інтерфейс»): ✕ - hidden for this target
    bool mine = false;    // the character hit it (not only it the character)
    ULONGLONG lastAt = 0;
    bool on = false, shown = false;
    float k = 0, scale = 1;
    int targetW = 0, targetH = 0;
    int mob = 0;
    tc::Data data;
    tc::Layout layout;
    bool hover = false, hoverBook = false;
    std::map<std::pair<int, int>, panel::Picture> pictures;  // by monster and side
    int picturesFor = -2;  // 3.0.11-beta.76: win/pack_pictures' iconsStamp() they were made for
    std::string key;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
    ULONGLONG shownAt = 0, goneAt = 0;  // 3.0.11-beta.46: the concept's motion - it fades in and out, 4 px
    bool uiShown = false;  // 3.0.18-beta.18: drawn by uaUI this frame
};

State& state() {
    static State s;
    return s;
}

uint32_t own() { return read32(kOwnAccount); }

// The class of the world's actor with this id; -1 none (gone, out of sight)
int classOf(uint32_t id) {
    const uint32_t mode = readGameMode(), world = mode ? read32(mode + kWorld) : 0;
    const uint32_t head = world ? read32(world + kActors) : 0;
    if (!head) return -1;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        const uint32_t actor = read32(node + 8);
        if (actor && read32(actor + kActorId) == id) return int(read32(actor + kActorClass));
    }
    return -1;
}

// The lowest bottom (the game's pixels) of the game's shown windows over the box (x0..x1, y0..y1); 0 none
int windowsOver(int x0, int y0, int x1, int y1) {
    const uint32_t head = read32(kWindowMgr + kWindows);
    if (!head) return 0;
    int bottom = 0;
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        const uint32_t window = read32(node + 8);
        int32_t r[4] = {0, 0, 0, 0};
        if (!window || !read32(window + kShown) || !readGame(window + kRect, r, sizeof r)) continue;
        if (r[2] <= 0 || r[3] <= 0 || r[0] >= x1 || r[0] + r[2] <= x0 || r[1] >= y1 || r[1] + r[3] <= y0) continue;
        bottom = std::max(bottom, r[1] + r[3]);
    }
    return bottom;
}

const panel::Picture* pictureOf(State& s, const refindex::Mob& mob, int id, int side) {
    if (const int made = packpictures::iconsStamp(); made != s.picturesFor) {  // 3.0.11-beta.76: the pack's frames
        s.picturesFor = made;
        s.pictures.clear();
    }
    const auto key = std::make_pair(id, side);
    auto it = s.pictures.find(key);
    if (it == s.pictures.end()) {
        if (s.pictures.size() > 64) s.pictures.clear();
        panel::Picture p;
        std::string from;
        if (monsterPicture(id, mob, side, p, &from) && s.told < 20) {
            ++s.told;
            LOG_INFO("target: %s's picture - %s", mob.name.c_str(), from.c_str());
        }
        it = s.pictures.emplace(key, std::move(p)).first;
    }
    return it->second.width > 0 ? &it->second : nullptr;
}

tc::Texts texts() {
    const settings::Lang l = language();
    return tc::Texts{settings::tr(l, settings::Text::TargetWeak), settings::tr(l, settings::Text::TargetHeals),
                     settings::tr(l, settings::Text::TargetBook),
                     settings::tr(l, settings::Text::TargetYourBar),  // 3.0.11-beta.68
                     settings::tr(l, settings::Text::TargetPadAttack), settings::tr(l, settings::Text::TargetPadNext)};
}

bool toTarget(const State& s, HWND game, int x, int y, float& tx, float& ty) {
    RECT client{};
    if (!GetClientRect(game, &client) || client.right <= 0 || client.bottom <= 0) return false;
    tx = float(x) * float(s.targetW) / float(client.right);  // Windows' size of the window (uaRO.exe is not DPI-aware)
    ty = float(y) * float(s.targetH) / float(client.bottom);
    return true;
}

void uiRect(float& x, float& y, float& w, float& h);

void block(const State& s, HWND game) {  // the mouse's clicks on the card are its own
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    panel::Box b = s.layout.card;
    float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    uiRect(x0, y0, b.w, b.h);  // 3.0.18-beta.53: as uaUI draws it - its clicks and the editor's frame
    const RECT r = clientBox(client, s.targetW, s.targetH, x0, y0, b.w, b.h);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Target, game, &r, false);
}

void hide(State& s) {
    s.uiShown = false;
    if (s.shown) {
        uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Target, nullptr, nullptr, false);
        s.goneAt = GetTickCount64();  // its last picture fades out
    }
    s.shown = false;
    s.hover = s.hoverBook = false;
}

void forget(State& s, const char* why) {
    if (s.target && s.mob && s.told < 40) {
        ++s.told;
        LOG_INFO("target: %s (#%d) %s", s.data.name.c_str(), s.mob, why);
    }
    s.target = 0;
    s.mob = 0;
}

std::string joined(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& p : parts) out += (out.empty() ? "" : ", ") + p;
    return out;
}

void describe(tc::Data& d, const refindex::Mob& m, int id) {
    const settings::Lang l = language();
    d.mob = id;
    d.name = m.name;
    d.level = m.lv;
    d.element = refpages::elementName(m.element, l) + " " + std::to_string(m.elementLv);
    d.elementRgb = refpages::elementColour(m.element);
    const std::string dot = " \xC2\xB7 ";
    d.kind = refpages::raceName(m.race, l) + dot + refpages::sizeName(m.size, l) + (m.boss ? dot + "Boss" : "");
    d.numbers = "HP " + refpages::number(m.hp, l) + dot + "Base " + refpages::number(m.baseExp, l) + dot + "Job " +
                refpages::number(m.jobExp, l);
    const tc::Weakness w = tc::weakness(m.element, m.elementLv);
    std::vector<std::string> weak, heals;
    for (const auto& [e, per] : w.weak) weak.push_back(refpages::elementName(e, l) + " " + std::to_string(per) + " %");
    for (const int e : w.heals) heals.push_back(refpages::elementName(e, l));
    d.weak = joined(weak);
    d.heals = joined(heals);
    // 3.0.11-beta.68 (the concept's «карточка цели со строкой «Ваша панель»»): the hotbar's skills on it
    d.bar = barhint::line(barhint::against(hotbarwin::barSkills(), m.element, m.elementLv), l, 3);
}


// ---- 3.0.18-beta.18 (the owner: «Переписывать 73 части в разметку RmlUi вручную»): the card as an RmlUi document
// while [interface] uaui is on - its words and colours core targetcard's, its details shown while pointed at (the
// theme's :hover), a click opens its card in «Довідник»; its picture not painted then ----

const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="tc away" data-model="target" data-style-width="w + 'px'">
<div class="card" data-event-click="open()">
 <div class="corner tl"></div><div class="corner tr"></div><div class="corner bl"></div><div class="corner br"></div>
 <div class="round"><img data-if="pic != ''" data-attr-src="pic"/></div>
 <div class="mid">
  <div class="top" data-class-withhp="hp >= 0"><span class="name">{{name}}</span><span class="lv">{{level}}</span><span
   class="chip" data-if="element != ''" data-style-color="ecol" data-style-border-color="eedge"
   data-style-background-color="ebg">{{element}}</span></div>
  <div class="hpline" data-if="hp >= 0"><span class="hptrack"><span class="hpfill" data-class-low="hp < 25"
   data-style-width="hp_bar + '%'"></span></span><span class="pct">{{hp}}%</span></div>
 </div>
 <div class="book"><img class="dim" data-attr-src="book"/><img class="lit" data-attr-src="book_lit"/></div>
 <div class="tcx" data-event-click="close()">&#x2715;</div>
 <div class="hints" data-if="pad"><div><img data-attr-src="pad_a"/>{{attack}}</div><div><img data-attr-src="pad_r"/>{{next}}</div></div>
</div><br/>
<div class="details"><p data-for="l, i : lines" data-if="l.text != ''" data-style-color="l.colour"
  data-class-first="i == 0">{{l.text}}</p></div>
</body></rml>)RML";

struct Line {
    Rml::String text, colour = "#eceff5";
};

struct Ui {
    uaui::Doc doc;
    float w = 0;
    Rml::String name, level, element, pic, picSrc, book, bookLit, padA, padR, attack, next;
    // 3.0.18-beta.62 (R-BK5SWP «border-color: ;»): never an empty style value, even before the first fill
    Rml::String ecol = "#00000000", eedge = "#00000000", ebg = "#00000000";
    int hp = -1;
    float hpBar = 0;  // 3.0.18-beta.58: the bar's shown share (gliding with [interface] uaui_motion)
    uiglide::Glide hpGlide;
    Rml::String glidFor;
    bool pad = false;
    std::vector<Line> lines;
    std::string key;
    panel::Picture bookPic[2], glyph[2];
    std::string bookSrc[2], glyphSrc[2];
    float iconsPx = 0;
    bool sony = false;
    int gen = 0;
    bool shown = false;
};

Ui& ui() {
    static Ui u;
    return u;
}

// 3.0.18-beta.49: the size uaUI draws it at last frame (its document times its size in «Розміщення інтерфейсу») - the
// editor's frame round what is seen; untouched while uaUI does not draw it
void uiSize(int& w, int& h) {
    int x = 0, y = 0, dw = 0, dh = 0;
    if (!uaui::ready() || !ui().doc.box(x, y, dw, dh)) return;
    w = dw, h = dh;  // 3.0.18-beta.222: laid out at its size
}

// The place and size uaUI draws it at last frame (grown from its top middle), in the target's pixels; untouched while
// uaUI does not draw it
void uiRect(float& x, float& y, float& w, float& h) {
    int dx = 0, dy = 0, dw = 0, dh = 0;
    if (!uaui::ready() || !ui().doc.box(dx, dy, dw, dh)) return;
    x = float(dx), y = float(dy), w = float(dw), h = float(dh);  // 3.0.18-beta.222: laid out at its size
}

std::string colourOf(uint32_t rgb, int alpha = -1) {
    char b[16];
    if (alpha < 0) std::snprintf(b, sizeof b, "#%06x", unsigned(uaui::themed(rgb) & 0xFFFFFF));
    else std::snprintf(b, sizeof b, "#%06x%02x", unsigned(uaui::themed(rgb) & 0xFFFFFF), alpha & 0xFF);
    return b;
}

bool uiFrame(State& s) {
    Ui& u = ui();
    if (!uaui::ready()) {
        u.doc.show(false);
        return false;
    }
    if (!u.doc.made() && !u.doc.make("target", kDoc, [&](Rml::DataModelConstructor& m) {
            if (auto l = m.RegisterStruct<Line>()) {
                l.RegisterMember("text", &Line::text);
                l.RegisterMember("colour", &Line::colour);
            }
            m.RegisterArray<std::vector<Line>>();
            m.Bind("w", &u.w);
            m.Bind("name", &u.name);
            m.Bind("level", &u.level);
            m.Bind("element", &u.element);
            m.Bind("ecol", &u.ecol);
            m.Bind("eedge", &u.eedge);
            m.Bind("ebg", &u.ebg);
            m.Bind("hp", &u.hp);
            m.Bind("hp_bar", &u.hpBar);
            m.Bind("pic", &u.pic);
            m.Bind("book", &u.book);
            m.Bind("book_lit", &u.bookLit);
            m.Bind("pad", &u.pad);
            m.Bind("pad_a", &u.padA);
            m.Bind("pad_r", &u.padR);
            m.Bind("attack", &u.attack);
            m.Bind("next", &u.next);
            m.Bind("lines", &u.lines);
            m.BindEventCallback("close", [](Rml::DataModelHandle, Rml::Event& e, const Rml::VariantList&) {
                State& s = state();  // 3.0.18-beta.120: ✕ - hidden till the next target
                s.dismissed = s.target;
                e.StopPropagation();
                if (s.told < 40) {
                    ++s.told;
                    LOG_INFO("target: the card closed by its ✕ for the actor %u", s.target);
                }
            });
            m.BindEventCallback("open", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
                State& s = state();
                if (s.mob <= 0) return;
                referencewin::openMob(s.mob);  // its card in «Довідник» (when it is on)
                if (s.told < 40) {
                    ++s.told;
                    LOG_INFO("target: %s's card asked for from the target's card (uaUI)", s.data.name.c_str());
                }
            });
        }))
        return false;
    const tc::Texts t = texts();
    const float dp = uaui::dp() * hudedit::sizeOf(hudlayout::Part::Target);
    const std::string key = std::to_string(s.mob) + ';' + std::to_string(s.data.hp) + ';' +
                            (s.data.picture ? "p" : "") + (s.data.pad ? (s.data.padSony ? "S" : "X") : "") + ';' +
                            std::to_string(s.layout.w) + '#' + std::to_string(uistyle::generation());
    if (key != u.key) {
        u.key = key;
        const tc::Data& d = s.data;
        u.w = float(s.layout.w);
        u.name = d.name;
        u.level = "Lv " + std::to_string(d.level);
        u.element = d.element;
        u.ecol = colourOf(d.elementRgb);
        u.eedge = colourOf(d.elementRgb, 153);  // its edge 0.6, its ground 0.22, as the picture's
        u.ebg = colourOf(d.elementRgb, 56);
        u.hp = d.hp;
        u.pad = d.pad && !t.attack.empty();
        u.attack = t.attack;
        u.next = t.next;
        // 3.0.18-beta.127 (R-7TVEJU «lines[4].colour», «color: ;»): never fewer rows than before - a shorter list
        // left RmlUi a row past its end for a frame; the rows over the words empty and hidden
        const size_t had = u.lines.size();
        u.lines.clear();
        for (const auto& [text, rgb] : tc::detailLines(d, t)) u.lines.push_back({text, colourOf(rgb)});
        while (u.lines.size() < std::max<size_t>(had, 12)) u.lines.push_back(Line{});
        u.doc.dirtyAll();
    }
    // the monster's picture, the book's icon (dim and lit), the controller's buttons - pictures of the driver's
    const Rml::String pic = uaui::picture(s.data.picture, s.mob, u.picSrc);
    if (pic != u.pic) u.pic = pic, u.doc.dirty("pic");
    if (u.glidFor != u.name) u.glidFor = u.name, u.hpGlide.started = false;  // another one: its bar at once
    const float bar = u.hpGlide.step(float(std::max(u.hp, 0)), GetTickCount64(), uaui::motion());
    if (bar != u.hpBar) u.hpBar = bar, u.doc.dirty("hp_bar");
    const float px = 18 * dp;
    if (px != u.iconsPx || u.sony != s.data.padSony) {
        u.iconsPx = px;
        u.sony = s.data.padSony;
        ++u.gen;
        for (int i = 0; i < 2; ++i) {
            panel::Picture& b = u.bookPic[i];
            const int side = std::max(8, int(std::ceil(px)));
            b.width = b.height = side;
            b.px.assign(size_t(side) * size_t(side), 0u);
            uiicons::draw(b, uiicons::Icon::Quest, 0, 0, float(side), i ? 0xECEFF5 : 0x9AA3B5, 1.0f, 2.0f);
            const float gpx = 17 * dp;
            panel::Picture& g = u.glyph[i];
            const int gs = std::max(8, int(std::ceil(gpx)));
            g.width = g.height = gs;
            g.px.assign(size_t(gs) * size_t(gs), 0u);
            const padglyph::Family f = u.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
            const padglyph::Button button = i ? padglyph::Button::R3 : padglyph::Button::A;
            padglyph::paint(g, button, f, 0, 0, float(gs));
            const padglyph::Letter l = padglyph::letter(button, f, 0, 0, float(gs));
            if (!l.text.empty())
                drawWords(g, {Words{l.box, utf8Wide(l.text), l.rgb, l.px, true, DT_CENTER, false}});
        }
        u.book = uaui::picture(&u.bookPic[0], u.gen, u.bookSrc[0]);
        u.bookLit = uaui::picture(&u.bookPic[1], u.gen, u.bookSrc[1]);
        u.padA = uaui::picture(&u.glyph[0], u.gen, u.glyphSrc[0]);
        u.padR = uaui::picture(&u.glyph[1], u.gen, u.glyphSrc[1]);
        u.doc.dirtyAll();
    }
    if (u.doc.doc()) {
        // its size in «Розміщення інтерфейсу» - 3.0.18-beta.222: its document's own dp scale (laid out and its letters
        // made at that size), not transform: scale (the words shrunk as a picture, R-BN9TJA's kind)
        u.doc.zoom(hudedit::sizeOf(hudlayout::Part::Target));
    }
    u.doc.place(s.layout.x, s.layout.y);
    u.doc.show(true);
    return true;
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    const uint32_t me = own();
    if (!me) return;
    const ULONGLONG now = GetTickCount64();
    if (const auto* h = std::get_if<packets::Hit>(&e)) {
        if (h->source == me && h->target != me && h->target) {  // the character hits it: its target
            if (h->target != s.target) s.mob = 0;
            s.target = h->target;
            s.mine = true;
            s.lastAt = now;
        } else if (h->target == me && h->source != me && h->source) {  // it hits the character
            if (!s.target || (!s.mine && h->source != s.target) || now - s.lastAt > kForgetMs) {
                if (h->source != s.target) s.mob = 0;
                s.target = h->source;
                s.mine = false;
            }
            if (h->source == s.target) s.lastAt = now;
        }
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        if (v->id == s.target && s.target) forget(s, v->type == 1 ? "defeated" : "out of sight");
    }
}

uint32_t fighting() {
    const State& s = state();
    return s.target && GetTickCount64() - s.lastAt <= kForgetMs ? s.target : 0;
}

void frame(HWND game, bool on, bool padHints, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        LOG_INFO("target: [interface] target = %d", on ? 1 : 0);
    }
    if (!on || !game || k <= 0 || targetWidth <= 0 || targetHeight <= 0 || !inGameWorld()) {
        hide(s);
        return;
    }
    const ULONGLONG now = GetTickCount64();
    if (s.target && now - s.lastAt > kForgetMs) forget(s, "left alone");
    const auto index = reference::index();
    if (!s.target || !index || s.target == s.dismissed ||
        ownwin::closed(closedwin::Window::Target)) {  // 3.0.18-beta.120: ✕ for this one; or closed by its icon
        hide(s);
        return;
    }
    const int cls = classOf(s.target);
    if (cls < 0) {
        forget(s, "gone");
        hide(s);
        return;
    }
    const auto m = index->mobs.find(cls);
    if (m == index->mobs.end()) {  // a player, an NPC: not a monster's card
        hide(s);
        return;
    }
    const float scale = uistyle::partScale(k) * hudedit::sizeOf(hudlayout::Part::Target);  // 3.0.11-beta.75: its size
    s.k = float(k);
    s.scale = scale;
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    if (cls != s.mob) {
        s.mob = cls;
        describe(s.data, m->second, cls);
        if (s.told < 40) {
            ++s.told;
            LOG_INFO("target: %s (#%d, Lv %d, %s) - the actor %u the character %s", m->second.name.c_str(), cls,
                     m->second.lv, s.data.element.c_str(), s.target, s.mine ? "hits" : "is hit by");
        }
    }
    const int side = int(std::lround(32 * scale));
    s.data.picture = pictureOf(s, m->second, cls, side);
    const uapad::runtime::Look pad = padHints ? uapad::runtime::look() : uapad::runtime::Look{};  // 3.0.11-beta.86
    s.data.pad = pad.shown;
    s.data.padSony = pad.sony;
    s.data.hp = mobhpwin::hp(s.target);  // 3.0.11-beta.95: its HP bar, from its name line's per cent
    // its place: the top middle, under the game's own windows standing there
    const tc::Texts t = texts();
    tc::Layout L = tc::layout(s.data, t, targetWidth / 2, int(std::lround(10 * scale)), scale, s.hover);
    const float kk = float(k);
    const int under = windowsOver(int(float(L.x) / kk), int(float(L.y) / kk), int(float(L.x + L.w) / kk),
                                  int(float(L.y) / kk + L.card.h / kk));
    if (under > 0) L = tc::layout(s.data, t, targetWidth / 2, int(std::lround(float(under + 6) * kk)), scale, s.hover);
    {  // 3.0.18-beta.49 (R-786CGH: the editor's frame narrower than the card): uaUI's card's own size
        int w = L.w, h = L.h;
        uiSize(w, h);
        hudedit::place(hudlayout::Part::Target, kk, targetWidth, targetHeight, w, h, L.x, L.y);  // 3.0.11-beta.75
    }
    s.layout = L;
    int cx = 0, cy = 0;
    bool hover = false, hoverBook = false;
    if (readHoverCursor(cx, cy)) {
        const tc::Part p =
            tc::hitTest(s.layout, float(cx) * kk - float(s.layout.x), float(cy) * kk - float(s.layout.y));
        hover = p != tc::Part::None;
        hoverBook = p == tc::Part::Book;
    }
    if (hover != s.hover) {  // its details under it: laid out again
        s.hover = hover;
        s.layout = tc::layout(s.data, t, targetWidth / 2, s.layout.y, scale, hover);
    }
    s.hoverBook = hoverBook;
    const std::string key = std::to_string(cls) + ';' + std::to_string(s.layout.w) + 'x' + std::to_string(s.layout.h) +
                            ';' + std::to_string(scale) + ';' + (hover ? "h" : "") + (hoverBook ? "b" : "") +
                            (s.data.picture ? "p" : "") + (s.data.pad ? (s.data.padSony ? "S" : "X") : "") + ';' +
                            std::to_string(s.data.hp);
    s.uiShown = uiFrame(s);  // 3.0.18-beta.18: by uaUI while [interface] uaui is on
    if (!s.uiShown) {
        if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {
            s.key = drawn;  // its grounds
        }
    }
    if (!s.shown) s.shownAt = now;
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown) ui().doc.show(false);  // 3.0.18-beta.18: gone - its document fades out by the theme
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

int actorClass(uint32_t id) { return classOf(id); }

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown || s.uiShown) return false;  // 3.0.18-beta.18: uaUI's events take its clicks
    const bool button = mouseButton(m);
    if (!button && m != WM_MOUSEWHEEL) return false;
    (void)wp;
    POINT p{GET_X_LPARAM(lp), GET_Y_LPARAM(lp)};
    if (m == WM_MOUSEWHEEL && !ScreenToClient(game, &p)) return false;  // the wheel's point is the screen's
    float tx = 0, ty = 0;
    if (!toTarget(s, game, p.x, p.y, tx, ty)) return false;
    const tc::Part part = tc::hitTest(s.layout, tx - float(s.layout.x), ty - float(s.layout.y));
    if (part == tc::Part::None) return false;
    if (m == WM_LBUTTONDOWN && s.mob > 0) {
        referencewin::openMob(s.mob);  // its card in «Довідник» (when it is on)
        if (s.told < 40) {
            ++s.told;
            LOG_INFO("target: %s's card asked for from the target's card", s.data.name.c_str());
        }
    }
    result = 0;
    return true;
}

}  // namespace uagfx::win::targetpanel
