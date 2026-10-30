#include "win/low_hp_edge.hpp"

#include <windows.h>

#include <algorithm>

#include "core/char_status.hpp"
#include "core/ui_motion.hpp"
#include "core/ui_style.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"

namespace uagfx::win::lowhpedge {
namespace {

// The concept's (build12 .vign): rgba(255, 45, 35, .40) at the edge, fading in over about a third of the screen
constexpr uint32_t kRed = 0xFF2D23;
constexpr float kAlpha = 0.40f, kDepth = 0.33f;
constexpr int kW = 256, kH = 144;  // stretched over the screen, smoothly

struct State {
    int strength = 0;
    bool low = false;
    float k = 0, opacity = 0;
    int targetW = 0, targetH = 0;
    ULONGLONG changedAt = 0;
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = false;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

void frame(int strength, double k, int targetWidth, int targetHeight) {
    State& s = state();
    strength = std::clamp(strength, 0, 100);
    if (strength != s.strength) {
        s.strength = strength;
        LOG_INFO("low HP edge: [interface] low_hp_edge = %d", strength);
    }
    charstatus::Status st;
    const bool low = strength > 0 && inGameWorld() && readOwnStatus(st) && st.hpMost > 0 && st.hp > 0 &&
                     int64_t(st.hp) * 4 < int64_t(st.hpMost);
    const ULONGLONG now = GetTickCount64();
    if (low != s.low) {
        s.low = low;
        s.changedAt = now;
        if (s.told < 10 && low) {
            ++s.told;
            LOG_INFO("low HP edge: the screen's edges red (%d %%) - HP %d of %d", strength, st.hp, st.hpMost);
        }
    }
    s.k = float(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const float fade = low ? motion::fadeIn(now, s.changedAt, motion::kToastInMs)
                           : motion::fadeOut(now, s.changedAt, motion::kToastOutMs);
    s.opacity = float(strength) / 100.0f * fade;
    if (s.opacity > 0 && s.picture.width == 0) {
        s.picture = uistyle::vignette(kW, kH, kRed, kAlpha, kDepth);
        s.upload = true;
    }
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (s.opacity <= 0 || s.k <= 0 || s.targetW <= 0 || s.targetH <= 0 || s.picture.width <= 0) return;
    const size_t world = record.firstInterface();  // where the world ends: the edge goes there
    const size_t before = record.commands().size();
    drawStretched(record, renderer, s.picture, s.texture, s.texW, s.texH, s.upload, 0, 0, s.targetW, s.targetH, s.k,
                  s.opacity);
    const size_t added = record.commands().size() - before;  // its upload too, when it went now
    if (added) record.moveLastBefore(added, world);
}

}  // namespace uagfx::win::lowhpedge
