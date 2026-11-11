#include "win/exp_gain.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "core/ui_style.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/uaui.hpp"

namespace uagfx::win::expgainwin {
namespace {

constexpr uint32_t kOwnAccount = 0x771F7C;
constexpr uint32_t kGold = 0xF3D27A;

struct Shown {  // a floater's picture
    std::string text;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0, x = 0, y = 0;
    bool upload = false;
    float alpha = 0;
};

struct State {
    bool on = true;
    expgain::Tracker tracker;
    uint32_t me = 0;
    float k = 0;
    std::vector<Shown> shown;
    int told = 0;
    bool uiShown = false;  // 3.0.18-beta.24: drawn by uaUI
    int targetW = 0, targetH = 0;
    float scale = 1;
};

State& state() {
    static State s;
    return s;
}

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

}  // namespace

void packet(const packets::Event& e) {
    State& s = state();
    const uint32_t me = read32(kOwnAccount);
    if (!me) return;
    if (me != s.me) {  // another character
        s.me = me;
        s.tracker.reset();
    }
    const uint64_t now = GetTickCount64();
    if (const auto* h = std::get_if<packets::Hit>(&e)) {
        if (h->source == me && h->target && h->target != me) s.tracker.hit(h->target, now + h->delay);
    } else if (const auto* v = std::get_if<packets::Vanish>(&e)) {
        uint64_t shownAt = 0;
        if (v->type != 1 || v->id == me || !s.tracker.died(v->id, &shownAt)) return;
        float box[4] = {};
        if (readActorBox(v->id, box)) s.tracker.killedAt(v->id, (box[0] + box[2]) / 2, box[1], now, shownAt);
    } else if (const auto* x = std::get_if<packets::StatusLong>(&e)) {
        if (x->type == 1 || x->type == 2) s.tracker.experience(x->type, x->value, now);
    }
}

static void frameInner(State& s, bool on, double k, int targetWidth, int targetHeight) {
    if (on != s.on) {
        s.on = on;
        LOG_INFO("experience gains: [interface] exp_gain = %d", on ? 1 : 0);
    }
    const uint64_t now = GetTickCount64();
    s.tracker.step(now);
    s.k = float(k);
    const std::vector<expgain::Floater>& fl = s.tracker.floaters();
    if (!on || !inGameWorld() || k <= 0 || targetWidth <= 0 || targetHeight <= 0) {
        for (Shown& sh : s.shown) sh.alpha = 0;
        return;
    }
    if (s.shown.size() < 8) s.shown.resize(8);  // a pool kept: their textures reused, never let go
    for (size_t i = fl.size(); i < s.shown.size(); ++i) s.shown[i].alpha = 0;
    const float scale = uistyle::partScale(k);
    s.targetW = targetWidth, s.targetH = targetHeight, s.scale = scale;
    const char decimal = ',';
    for (size_t i = 0; i < fl.size() && i < s.shown.size(); ++i) {
        const expgain::Floater& f = fl[i];
        Shown& sh = s.shown[i];
        const std::string text = expgain::text(f.base, f.job, decimal);
        if (text != sh.text && s.told < 10) {
            ++s.told;
            LOG_INFO("experience gains: %s over the monster", text.c_str());
        }
        sh.text = text;
        sh.x = int(std::lround(f.x * float(k)));  // uaUI's: its middle
        sh.y = int(std::lround((f.y - expgain::rise(f, now)) * float(k)));
        sh.alpha = expgain::opacity(f.ms, now, expgain::kFloatMs, expgain::kFadeMs);
    }
}

namespace {

// 3.0.18-beta.24 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 4 - by hand): the gains over the monsters
// in RmlUi's markup - each a gold word with its outline, its middle at the monster, rising and fading; drawn only
const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="xg" data-model="expgain" data-style-width="w + 'px'" data-style-height="h + 'px'">
<span class="gain" data-for="f : floaters" data-style-left="f.x - 300 * s + 'px'" data-style-top="f.y - 22 * s + 'px'"
  data-style-opacity="f.op">{{f.text}}</span>
</body></rml>)RML";

struct FloaterV {
    float x = 0, y = 0, op = 0;
    Rml::String text;
};

struct Ui {
    uaui::Doc doc;
    float w = 0, h = 0, s = 1;
    std::vector<FloaterV> floaters;
    size_t count = 0;
};

Ui& ui() {
    static Ui u;
    return u;
}

bool uiFrame(State& s) {
    Ui& u = ui();
    std::vector<FloaterV> now;
    for (const Shown& sh : s.shown)
        if (sh.alpha > 0 && !sh.text.empty()) now.push_back(FloaterV{float(sh.x), float(sh.y), sh.alpha, sh.text});
    if (!uaui::ready() || !s.on || s.k <= 0 || now.empty()) {
        u.doc.show(false);
        return uaui::ready() && s.on;
    }
    if (!u.doc.made()) {
        u.doc.passive = true;
        if (!u.doc.make("exp gain", kDoc, [&](Rml::DataModelConstructor& m) {
                if (auto f = m.RegisterStruct<FloaterV>()) {
                    f.RegisterMember("x", &FloaterV::x);
                    f.RegisterMember("y", &FloaterV::y);
                    f.RegisterMember("op", &FloaterV::op);
                    f.RegisterMember("text", &FloaterV::text);
                }
                m.RegisterArray<std::vector<FloaterV>>();
                m.Bind("w", &u.w);
                m.Bind("h", &u.h);
                m.Bind("s", &u.s);
                m.Bind("floaters", &u.floaters);
            }))
            return false;
    }
    u.w = float(s.targetW), u.h = float(s.targetH), u.s = s.scale;
    u.floaters = std::move(now);
    u.doc.dirtyAll();  // they rise each frame
    u.doc.place(0, 0);
    u.doc.show(true);
    return true;
}

}  // namespace

void frame(bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    frameInner(s, on, k, targetWidth, targetHeight);
    s.uiShown = uiFrame(s);
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    (void)record;  // the old edition (its pictures) gone: uaUI draws the floaters
    (void)renderer;
}

expgain::Label label() {
    const State& s = state();
    return s.on ? s.tracker.label() : expgain::Label{};
}

}  // namespace uagfx::win::expgainwin
