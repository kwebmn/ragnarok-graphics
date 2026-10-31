#include "win/target_card.hpp"

#include <windowsx.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <variant>
#include <vector>

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
#include "win/monster_picture.hpp"
#include "win/pack_pictures.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"
#include "win/reference_load.hpp"
#include "win/reference_window.hpp"
#include "win/ui_cost.hpp"

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
};

State& state() {
    static State s;
    return s;
}

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
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
        if (monsterPicture(mob, side, p, &from) && s.told < 20) {
            ++s.told;
            LOG_INFO("target: %s's picture - %s", mob.name.c_str(), from.c_str());
        }
        it = s.pictures.emplace(key, std::move(p)).first;
    }
    return it->second.width > 0 ? &it->second : nullptr;
}

tc::Texts texts() {
    const settings::Lang l = startup().lang;
    return tc::Texts{settings::tr(l, settings::Text::TargetWeak), settings::tr(l, settings::Text::TargetHeals),
                     settings::tr(l, settings::Text::TargetBook),
                     settings::tr(l, settings::Text::TargetYourBar),  // 3.0.11-beta.68
                     settings::tr(l, settings::Text::TargetPadAttack), settings::tr(l, settings::Text::TargetPadNext)};
}

std::vector<Words> gdiWords(const std::vector<tc::Words>& in) {
    std::vector<Words> out;
    for (const tc::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == tc::Align::Right    ? DT_RIGHT
                                 : w.align == tc::Align::Center ? DT_CENTER
                                                                : DT_LEFT),
                            false});
    return out;
}

void render(State& s) {
    const uicost::Paint cost("target's card", s.layout.w, s.layout.h);  // 3.0.11-beta.55
    if (!panel::blank(s.picture, s.layout.w, s.layout.h)) return;  // 3.0.11-beta.57: in the memory it had
    const tc::Texts t = texts();
    tc::paint(s.picture, s.layout, s.data, s.hoverBook);
    drawWords(s.picture, gdiWords(tc::words(s.layout, s.data, t, s.hoverBook)));
    s.upload = true;
}

bool toTarget(const State& s, HWND game, int x, int y, float& tx, float& ty) {
    RECT client{};
    if (!GetClientRect(game, &client) || client.right <= 0 || client.bottom <= 0) return false;
    tx = float(x) * float(s.targetW) / float(client.right);  // Windows' size of the window (uaRO.exe is not DPI-aware)
    ty = float(y) * float(s.targetH) / float(client.bottom);
    return true;
}

void block(const State& s, HWND game) {  // the mouse's clicks on the card are its own
    RECT client{};
    if (s.targetW <= 0 || s.targetH <= 0 || !GetClientRect(game, &client)) return;
    const panel::Box& b = s.layout.card;
    const float x0 = float(s.layout.x) + b.x, y0 = float(s.layout.y) + b.y;
    const RECT r{LONG(int64_t(x0) * client.right / s.targetW), LONG(int64_t(y0) * client.bottom / s.targetH),
                 LONG(int64_t(x0 + b.w) * client.right / s.targetW + 1),
                 LONG(int64_t(y0 + b.h) * client.bottom / s.targetH + 1)};
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::Target, game, &r, false);
}

void hide(State& s) {
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
    const settings::Lang l = startup().lang;
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
    if (!s.target || !index) {
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
    // its place: the top middle, under the game's own windows standing there
    const tc::Texts t = texts();
    tc::Layout L = tc::layout(s.data, t, targetWidth / 2, int(std::lround(10 * scale)), scale, s.hover);
    const float kk = float(k);
    const int under = windowsOver(int(float(L.x) / kk), int(float(L.y) / kk), int(float(L.x + L.w) / kk),
                                  int(float(L.y) / kk + L.card.h / kk));
    if (under > 0) L = tc::layout(s.data, t, targetWidth / 2, int(std::lround(float(under + 6) * kk)), scale, s.hover);
    hudedit::place(hudlayout::Part::Target, kk, targetWidth, targetHeight, L.w, L.h, L.x, L.y);  // 3.0.11-beta.75
    s.layout = L;
    int cx = 0, cy = 0;
    bool hover = false, hoverBook = false;
    if (readGameCursor(cx, cy)) {
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
                            (s.data.picture ? "p" : "") + (s.data.pad ? (s.data.padSony ? "S" : "X") : "");
    if (const std::string drawn = key + '#' + std::to_string(uistyle::generation()); drawn != s.key) {  // its grounds
        s.key = drawn;
        render(s);
    }
    if (!s.shown) s.shownAt = now;
    s.shown = true;
    block(s, game);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (s.k <= 0 || s.picture.width <= 0 || s.picture.height <= 0) return;
    // 3.0.11-beta.46: coming, from 4 px over its place; going, 4 px up out of sight ([interface] animations)
    const ULONGLONG now = GetTickCount64();
    const float a = s.shown ? motion::fadeIn(now, s.shownAt, motion::kCardMs)
                            : motion::fadeOut(now, s.goneAt, motion::kCardMs);
    if (a <= 0) return;
    const int shift = int(std::lround(-(1 - a) * motion::kCardShift * s.scale));
    int cursor = -1, cx = 0, cy = 0;  // under the game's own cursor, as the minimap
    if (readGameCursor(cx, cy)) cursor = panel::cursorDraw(record, float(cx), float(cy));
    drawPart(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, s.layout.x, s.layout.y + shift, s.k,
                a);
    if (cursor >= 0) record.moveLastBefore(record.commands().size() - size_t(cursor) - 1, size_t(cursor));
}

int actorClass(uint32_t id) { return classOf(id); }

bool mouse(HWND game, UINT m, WPARAM wp, LPARAM lp, LRESULT& result) {
    State& s = state();
    if (!s.shown) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
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
