#include "win/world_dim.hpp"

#include <windows.h>

#include "core/ui_motion.hpp"
#include "win/item_window.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/reference_window.hpp"
#include "win/stat_window.hpp"

namespace uagfx::win::worlddim {
namespace {

constexpr float kShade = 0.35f;

struct State {
    bool on = false, open = false;
    float k = 0, shade = 0;
    int targetW = 0, targetH = 0;
    ULONGLONG changedAt = 0;
    int told = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

bool windowsOpen() { return itemswin::shown() || statswin::shown() || referencewin::isOpen(); }

bool active() { return state().open; }

void frame(bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        LOG_INFO("dim: [interface] dim_world = %d", on ? 1 : 0);
    }
    const bool open = on && windowsOpen();
    const ULONGLONG now = GetTickCount64();
    if (open != s.open) {
        s.open = open;
        s.changedAt = now;
        if (s.told < 10) {
            ++s.told;
            LOG_INFO("dim: the world %s under the windows", open ? "dimmed" : "back");
        }
    }
    s.k = float(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    s.shade = kShade * (open ? motion::fadeIn(now, s.changedAt, motion::kBlockMs)
                             : motion::fadeOut(now, s.changedAt, motion::kBlockMs));
}

void draw(FrameRecord& record) {
    State& s = state();
    if (s.shade <= 0 || s.k <= 0 || s.targetW <= 0 || s.targetH <= 0) return;
    const size_t world = record.firstInterface();  // where the world ends: the shade goes there
    drawShade(record, 0, 0, s.targetW, s.targetH, s.k, s.shade);
    record.moveLastBefore(1, world);
}

}  // namespace uagfx::win::worlddim
