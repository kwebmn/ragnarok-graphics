#include "win/exp_bars.hpp"

#include <windows.h>

#include <algorithm>
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

namespace uagfx::win::expbars {
namespace {

namespace cb = uagfx::charblock;

constexpr ULONGLONG kTipMs = 300;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kText = 0xECEFF5, kMuted = 0x9AA3B5;

struct State {
    bool on = false, shown = false;
    float k = 0;
    int targetW = 0, targetH = 0;
    std::string key, tipKey;
    panel::Picture strip, tip;
    uint32_t stripTexture = 0, tipTexture = 0;
    int stripW = 0, stripH = 0, tipW = 0, tipH = 0, tipX = 0, tipY = 0;
    bool stripUpload = false, tipUpload = false, tipShown = false;
    ULONGLONG hoverSince = 0;
    bool hovered = false;
    // 3.0.11-beta.237: the last gain's label over the bars' right end (win/exp_gain), fading
    std::string gainKey;
    panel::Picture gain;
    uint32_t gainTexture = 0;
    int gainW = 0, gainH = 0, gainX = 0, gainY = 0;
    bool gainUpload = false;
    float gainAlpha = 0;
};

State& state() {
    static State s;
    return s;
}

}  // namespace

void frame(bool on, double k, int targetWidth, int targetHeight) {
    State& s = state();
    if (on != s.on) {
        s.on = on;
        LOG_INFO("experience bars: [interface] exp_bars = %d", on ? 1 : 0);
    }
    cb::Data d;
    cb::Texts t;
    s.shown = on && targetWidth > 0 && targetHeight > 0 && inGameWorld() && charpanel::expInfo(d, t) &&
              !cb::lastLevels(d) && (d.baseExp >= 0 || d.jobExp >= 0);
    s.tipShown = false;
    if (!s.shown) return;
    s.k = float(k);
    s.targetW = targetWidth;
    s.targetH = targetHeight;
    const float scale = uistyle::partScale(k);
    const int h = cb::stripHeight(scale);
    const std::string key = std::to_string(targetWidth) + 'x' + std::to_string(h) + ';' +
                            std::to_string(int(d.baseExp * 2000)) + ',' + std::to_string(int(d.jobExp * 2000)) + ';' +
                            std::to_string(d.baseNext) + ',' + std::to_string(d.jobNext);
    if (key != s.key) {
        s.key = key;
        panel::blank(s.strip, targetWidth, h);  // 3.0.18-beta.14
        cb::paintStrip(s.strip, d, scale);
        s.stripUpload = true;
    }
    s.gainAlpha = 0;  // 3.0.11-beta.237: the last gain by the bars, its part of each level
    const expgain::Label g = expgainwin::label();
    if (g.ms) {
        const double baseOf = g.base > 0 && d.baseNext > 0 ? double(g.base) / double(d.baseNext) : -1;
        const double jobOf = g.job > 0 && d.jobNext > 0 ? double(g.job) / double(d.jobNext) : -1;
        const std::string text = expgain::text(g.base, g.job, t.decimal, baseOf, jobOf);
        if (!text.empty()) {
            const std::string gkey = text + '@' + std::to_string(scale);
            if (gkey != s.gainKey) {
                s.gainKey = gkey;
                const std::wstring wide = utf8Wide(text);
                const float px = 13 * scale;
                const int w = wordsWidth(wide, px, true) + int(std::lround(12 * scale));
                const int hh = int(std::lround(20 * scale));
                panel::blank(s.gain, w, hh);  // 3.0.18-beta.14
                drawWords(s.gain, {Words{panel::Box{0, 0, float(w), float(hh)}, wide, 0xF3D27A, px, true, DT_CENTER,
                                         false, true}});
                s.gainUpload = true;
            }
            s.gainX = targetWidth - s.gain.width - int(12 * scale);
            s.gainY = targetHeight - h - s.gain.height - int(2 * scale);
            s.gainAlpha = expgain::opacity(g.ms, GetTickCount64(), expgain::kLabelMs, expgain::kFadeMs);
        }
    }
    int cx = 0, cy = 0;  // pointed at: the bottom 14 px (the strip and a little over it)
    const bool over = readGameCursor(cx, cy) && float(cy) * float(k) >= float(targetHeight) - 14.0f * scale;
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
    std::string tipKey = std::to_string(tw) + 'x' + std::to_string(th);
    for (const std::string& l : lines) tipKey += ';' + l;
    if (tipKey != s.tipKey) {
        s.tipKey = tipKey;
        panel::blank(s.tip, tw, th);  // 3.0.18-beta.14
        panel::roundRect(s.tip, panel::Box{0, 0, float(tw), float(th)}, 8 * scale, kCard, 0.94f, kWhite, 0.12f,
                         1.0f * scale);
        std::vector<Words> words;
        for (size_t i = 0; i < lines.size(); ++i)
            words.push_back(Words{
                panel::Box{10 * scale, (4 + 17 * float(i)) * scale, float(tw) - 20 * scale, 17 * scale},
                utf8Wide(lines[i]), i == 0 ? kText : kMuted, 13 * scale, true, DT_LEFT, false, false});
        drawWords(s.tip, words);
        s.tipUpload = true;
    }
    s.tipX = std::clamp(int(float(cx) * float(k)) - tw / 2, 0, std::max(0, targetWidth - tw));
    s.tipY = targetHeight - h - th - int(6 * scale);
    s.tipShown = true;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0 || s.strip.width <= 0) return;
    drawPart(record, renderer, s.strip, s.stripTexture, s.stripW, s.stripH, s.stripUpload, 0,
             s.targetH - s.strip.height, s.k);
    if (s.gainAlpha > 0 && s.gain.width > 0)  // 3.0.11-beta.237
        drawPart(record, renderer, s.gain, s.gainTexture, s.gainW, s.gainH, s.gainUpload, s.gainX, s.gainY, s.k,
                 s.gainAlpha);
    if (s.tipShown && s.tip.width > 0)
        drawPart(record, renderer, s.tip, s.tipTexture, s.tipW, s.tipH, s.tipUpload, s.tipX, s.tipY, s.k);
}

}  // namespace uagfx::win::expbars
