#include "win/exp_bars.hpp"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "core/char_block.hpp"
#include "core/ui_style.hpp"
#include "win/char_block.hpp"
#include "win/exp_gain.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/picture_draw.hpp"
#include "win/uaui.hpp"

namespace uagfx::win::expbars {
namespace {

namespace cb = uagfx::charblock;

constexpr ULONGLONG kTipMs = 300;

struct State {
    bool on = false, shown = false;
    float k = 0;
    int targetW = 0, targetH = 0;
    int tipX = 0, tipY = 0;
    bool tipShown = false;
    ULONGLONG hoverSince = 0;
    bool hovered = false;
    // 3.0.11-beta.237: the last gain's label over the bars' right end (win/exp_gain), fading
    float gainAlpha = 0;
    // 3.0.18-beta.23: what uaUI's strip shows (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 4)
    bool uiShown = false;
    float scale = 1;
    int stripPx = 0, tipWpx = 0;
    std::array<uagfx::charblock::StripBar, 2> bars{};
    std::string gainText;
    std::vector<std::string> tipLines;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

static void frameInner(State& s, bool on, double k, int targetWidth, int targetHeight) {
    if (on != s.on) {
        s.on = on;
        LOG_INFO("experience bars: [interface] exp_bars = %d", on ? 1 : 0);
    }
    cb::Data d;
    cb::Texts t;
    s.shown = on && targetWidth > 0 && targetHeight > 0 && inGameWorld() && charpanel::expInfo(d, t) &&
              !cb::lastLevels(d) && (d.baseExp >= 0 || d.jobExp >= 0);
    s.tipShown = false;
    s.gainText.clear();
    s.tipLines.clear();
    if (!s.shown) return;
    s.k = float(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const float scale = uistyle::partScale(k);
    const int h = cb::stripHeight(scale);
    s.scale = scale;
    s.stripPx = h;
    s.bars = cb::stripBars(d);
    s.gainAlpha = 0;  // 3.0.11-beta.237: the last gain by the bars, its part of each level
    const expgain::Label g = expgainwin::label();
    if (g.ms) {
        const double baseOf = g.base > 0 && d.baseNext > 0 ? double(g.base) / double(d.baseNext) : -1;
        const double jobOf = g.job > 0 && d.jobNext > 0 ? double(g.job) / double(d.jobNext) : -1;
        const std::string text = expgain::text(g.base, g.job, t.decimal, baseOf, jobOf);
        if (!text.empty()) {
            s.gainText = text;
            s.gainAlpha = expgain::opacity(g.ms, GetTickCount64(), expgain::kLabelMs, expgain::kFadeMs);
        }
    }
    int cx = 0, cy = 0;  // pointed at: the bottom 14 px (the strip and a little over it)
    const bool over = readHoverCursor(cx, cy) && float(cy) * float(k) >= float(targetHeight) - 14.0f * scale;
    const ULONGLONG now = GetTickCount64();
    if (over != s.hovered) {
        s.hovered = over;
        s.hoverSince = now;
    }
    if (!over || now - s.hoverSince < kTipMs) return;
    const std::vector<std::string> lines = cb::stripTip(d, t);
    if (lines.empty()) return;
    float w = 0;
    for (const std::string& l : lines) w = std::max(w, panel::wordsWidth(l, 13 * scale, true));
    const int tw = int(std::ceil(w + 20 * scale)), th = int(std::ceil((8 + 17 * float(lines.size())) * scale));
    s.tipLines = lines;
    s.tipWpx = tw;
    s.tipX = std::clamp(int(float(cx) * float(k)) - tw / 2, 0, std::max(0, targetWidth - tw));
    s.tipShown = true;
    s.tipY = targetHeight - h - th - int(6 * scale);  // .158: uaUI's tip alone (no picture)
}

namespace {

// 3.0.18-beta.23 (docs/superpowers/plans/2026-11-09-uaui-migration.md, stage 4 - by hand): the strip in RmlUi's
// markup - the dark band, the base's bar over the job's with a tick each 10 %, the last gain's label, the tip; drawn
// only (passive): the tip is still this part's by the cursor's place
const char* kDoc = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="xb" data-model="expbars" data-style-width="w + 'px'" data-style-height="h + 'px'">
<div class="band" data-style-top="h - sh + 'px'" data-style-height="sh + 'px'">
 <div class="row" data-style-top="s + 'px'" data-style-height="bh + 'px'">
  <div class="fill base" data-style-width="base.f * 100 + '%'" data-style-opacity="base.a"></div>
  <div class="tick" data-for="t : ticks" data-style-left="t + '%'"></div>
 </div>
 <div class="row" data-style-top="2 * s + bh + 'px'" data-style-height="bh + 'px'">
  <div class="fill job" data-style-width="job.f * 100 + '%'" data-style-opacity="job.a"></div>
  <div class="tick" data-for="t : ticks" data-style-left="t + '%'"></div>
 </div>
</div>
<span class="gain" data-if="gain_on" data-style-right="12 * s + 'px'" data-style-bottom="sh + 2 * s + 'px'"
  data-style-opacity="gain_op">{{gain}}</span>
<div class="tip" data-if="tip_on" data-style-left="tip_x + 'px'" data-style-bottom="sh + 6 * s + 'px'"
  data-style-width="tip_w + 'px'">
 <div class="line" data-for="l : lines">{{l}}</div>
</div>
</body></rml>)RML";

struct BarV {
    float f = 0, a = 1;
};

struct Ui {
    uaui::Doc doc;
    float w = 0, h = 0, sh = 0, bh = 0, s = 1, gainOp = 0, tipX = 0, tipW = 0;
    BarV base, job;
    Rml::String gain;
    bool gainOn = false, tipOn = false;
    std::vector<Rml::String> lines;
    std::vector<float> ticks{10, 20, 30, 40, 50, 60, 70, 80, 90};  // a tick each 10 %
    std::string key;
};

Ui& ui() {
    static Ui u;
    return u;
}

bool uiFrame(State& s) {
    Ui& u = ui();
    if (!uaui::ready() || !s.shown) {
        u.doc.show(false);
        return false;
    }
    if (!u.doc.made()) {
        u.doc.passive = true;
        if (!u.doc.make("exp bars", kDoc, [&](Rml::DataModelConstructor& m) {
                if (auto b = m.RegisterStruct<BarV>()) {
                    b.RegisterMember("f", &BarV::f);
                    b.RegisterMember("a", &BarV::a);
                }
                m.RegisterArray<std::vector<Rml::String>>();
                m.RegisterArray<std::vector<float>>();
                m.Bind("ticks", &u.ticks);
                m.Bind("w", &u.w);
                m.Bind("h", &u.h);
                m.Bind("sh", &u.sh);
                m.Bind("bh", &u.bh);
                m.Bind("s", &u.s);
                m.Bind("base", &u.base);
                m.Bind("job", &u.job);
                m.Bind("gain", &u.gain);
                m.Bind("gain_on", &u.gainOn);
                m.Bind("gain_op", &u.gainOp);
                m.Bind("tip_on", &u.tipOn);
                m.Bind("tip_x", &u.tipX);
                m.Bind("tip_w", &u.tipW);
                m.Bind("lines", &u.lines);
            }))
            return false;
    }
    u.w = float(s.targetW), u.h = float(s.targetH), u.s = s.scale;
    u.sh = float(s.stripPx), u.bh = (u.sh - 3 * s.scale) / 2;
    u.base = BarV{s.bars[0].fill, s.bars[0].alpha}, u.job = BarV{s.bars[1].fill, s.bars[1].alpha};
    u.gainOn = s.gainAlpha > 0 && !s.gainText.empty();
    u.gain = s.gainText;
    u.gainOp = s.gainAlpha;
    u.tipOn = s.tipShown && !s.tipLines.empty();
    u.tipX = float(s.tipX), u.tipW = float(s.tipWpx);
    std::string key;  // everything but the label's fade (its opacity alone, each frame)
    char b[160];
    std::snprintf(b, sizeof b, "%.0fx%.0f;%.2f;%.4f,%.2f;%.4f,%.2f;%d;%d,%.0f,%.0f", u.w, u.h, u.s, u.base.f, u.base.a,
                  u.job.f, u.job.a, int(u.gainOn), int(u.tipOn), u.tipX, u.tipW);
    key = b;
    key += s.gainText;
    for (const std::string& l : s.tipLines) key += '\n' + l;
    if (key != u.key) {
        u.key = key;
        u.lines.assign(s.tipLines.begin(), s.tipLines.end());
        u.doc.dirtyAll();
    } else if (u.gainOn) {
        u.doc.dirty("gain_op");
    }
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
    (void)record;  // the old edition (its picture) gone: uaUI draws it
    (void)renderer;
}

}  // namespace uagfx::win::expbars
