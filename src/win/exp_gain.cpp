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

void frame(bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
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
    const char decimal = ',';
    for (size_t i = 0; i < fl.size() && i < s.shown.size(); ++i) {
        const expgain::Floater& f = fl[i];
        Shown& sh = s.shown[i];
        const std::string text = expgain::text(f.base, f.job, decimal);
        if (text != sh.text || sh.picture.width <= 0) {
            sh.text = text;
            const std::wstring wide = utf8Wide(text);
            const float px = 15 * scale;
            const int w = wordsWidth(wide, px, true) + int(std::lround(12 * scale));
            const int h = int(std::lround(22 * scale));
            panel::blank(sh.picture, w, h);  // 3.0.18-beta.14
            drawWords(sh.picture, {Words{panel::Box{0, 0, float(w), float(h)}, wide, kGold, px, true, DT_CENTER,
                                         false, true}});
            sh.upload = true;
            if (s.told++ < 10) LOG_INFO("experience gains: %s over the monster", text.c_str());
        }
        sh.x = int(std::lround(f.x * float(k))) - sh.picture.width / 2;
        sh.y = int(std::lround((f.y - expgain::rise(f, now)) * float(k))) - sh.picture.height;
        sh.alpha = expgain::opacity(f.ms, now, expgain::kFloatMs, expgain::kFadeMs);
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.on || s.k <= 0) return;
    for (Shown& sh : s.shown)
        if (sh.alpha > 0 && sh.picture.width > 0)
            drawPart(record, renderer, sh.picture, sh.texture, sh.texW, sh.texH, sh.upload, sh.x, sh.y, s.k, sh.alpha);
}

expgain::Label label() {
    const State& s = state();
    return s.on ? s.tracker.label() : expgain::Label{};
}

}  // namespace uagfx::win::expgainwin
