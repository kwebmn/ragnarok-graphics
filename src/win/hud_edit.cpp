#include "win/hud_edit.hpp"

#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

#include "core/hud_layout_panel.hpp"
#include "core/layout.hpp"
#include "core/settings/strings.hpp"
#include "core/ui_style.hpp"
#include "pad/win/game_window.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/picture_draw.hpp"
#include "win/real_ddraw.hpp"

namespace uagfx::win::hudedit {
namespace {

using hudlayout::Part;
constexpr int kParts = hudlayout::kParts;
// The order the parts are drawn in (win/context): the last drawn is the one under the pointer first
constexpr Part kOnTop[] = {Part::Chat, Part::Hotbar, Part::Party, Part::Target, Part::Character,
                           Part::Buffs, Part::Tracker, Part::Minimap};
const settings::Text kNames[kParts] = {settings::Text::HudPartCharacter, settings::Text::HudPartMinimap,
                                       settings::Text::HudPartBuffs,     settings::Text::HudPartTracker,
                                       settings::Text::HudPartHotbar,    settings::Text::HudPartTarget,
                                       settings::Text::HudPartParty,     settings::Text::HudPartChat};

struct Drawn {  // a picture of the editor's, its texture
    panel::Picture picture;
    uint32_t texture = 0;
    int texW = 0, texH = 0;
    bool upload = true;
    std::string key;
};

struct State {
    bool read = false;
    hudlayout::Layout layout;
    std::array<int, kParts> stamps{};
    std::array<hudlayout::Box, kParts> boxes{};
    std::array<int, kParts> seen{};  // the frame each part was last placed in
    int frameNo = 1;
    bool on = false, shown = false;
    float k = 0;
    int targetW = 0, targetH = 0;
    int hover = -1, drag = -1, grabX = 0, grabY = 0, guideX = -1, guideY = -1;
    hudlayout::BarPart barHover = hudlayout::BarPart::None;
    hudlayout::Bar bar;
    Drawn barPicture, guide;
    std::array<Drawn, kParts> frames;
};

State& state() {
    static State s;
    return s;
}

std::string say(settings::Text t) { return settings::tr(startup().lang, t); }

void readLayout(State& s) {
    if (s.read) return;
    s.read = true;
    std::string text;
    if (readTextFile(layout::join(gameDir(), layout::kHudLayout), text)) s.layout = hudlayout::parse(text);
    int moved = 0;
    for (const hudlayout::Place& p : s.layout) moved += p.moved || std::fabs(p.size - 1) > 1e-3f;
    if (moved) LOG_INFO("interface layout: %d parts placed or sized by the player (%s)", moved, layout::kHudLayout);
}

void save(State& s, Part part, const char* what) {
    const std::string path = layout::join(gameDir(), layout::kHudLayout);
    const bool ok = writeFileAtomic(path, hudlayout::text(s.layout));
    if (int(part) < kParts) {
        const hudlayout::Place& p = s.layout[size_t(part)];
        LOG_INFO("interface layout: %s %s - %s, %d %%%s", hudlayout::keyOf(part), what,
                 p.moved ? ("at the screen's point " + std::to_string(p.anchor) + " " + std::to_string(int(p.dx)) +
                            ", " + std::to_string(int(p.dy)))
                               .c_str()
                         : "its own place",
                 int(std::lround(p.size * 100)), ok ? "" : " - NOT saved");
    } else {
        LOG_INFO("interface layout: %s%s", what, ok ? "" : " - NOT saved");
    }
}

void touch(State& s, Part part) { ++s.stamps[size_t(part)]; }

bool shownNow(const State& s, int i) { return s.seen[size_t(i)] >= s.frameNo - 1; }
// Shown before, not now (a target's card without a target): its frame where it was, faint - still moved
bool known(const State& s, int i) { return s.seen[size_t(i)] > 0; }

int partAt(const State& s, int x, int y) {
    for (const bool live : {true, false})
        for (const Part p : kOnTop) {
            const int i = int(p);
            const hudlayout::Box& b = s.boxes[size_t(i)];
            if (known(s, i) && shownNow(s, i) == live && x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h)
                return i;
        }
    return -1;
}

std::vector<Words> gdiWords(const std::vector<hudlayout::Words>& in) {
    std::vector<Words> out;
    for (const hudlayout::Words& w : in)
        out.push_back(Words{w.box, utf8Wide(w.text), w.rgb, w.px, w.bold,
                            UINT(w.align == hudlayout::Align::Right    ? DT_RIGHT
                                 : w.align == hudlayout::Align::Center ? DT_CENTER
                                                                       : DT_LEFT),
                            false});
    return out;
}

bool cursor(const State& s, int& x, int& y) {
    int cx = 0, cy = 0;
    if (!readGameCursor(cx, cy) || s.k <= 0) return false;
    x = int(std::lround(float(cx) * s.k));
    y = int(std::lround(float(cy) * s.k));
    return true;
}

void close(State& s, const char* by) {
    if (!s.on) return;
    s.on = false;
    s.drag = -1;
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::HudEdit, nullptr, nullptr, false);  // the world's again
    LOG_INFO("interface layout: closed (%s)", by);
}

}  // namespace

float sizeOf(Part part) {
    State& s = state();
    readLayout(s);
    return int(part) < kParts ? s.layout[size_t(part)].size : 1.0f;
}

void place(Part part, float k, int targetW, int targetH, int w, int h, int& x, int& y) {
    State& s = state();
    readLayout(s);
    if (int(part) >= kParts) return;
    hudlayout::apply(s.layout[size_t(part)], k, targetW, targetH, w, h, x, y);
    s.boxes[size_t(part)] = hudlayout::Box{x, y, w, h};
    s.seen[size_t(part)] = s.frameNo;
}

int stampOf(Part part) { return int(part) < kParts ? state().stamps[size_t(part)] : 0; }

bool on() { return state().on; }

void open(HWND, const char* by) {
    State& s = state();
    readLayout(s);
    if (s.on) return;
    s.on = true;
    s.drag = -1;
    s.hover = -1;
    LOG_INFO("interface layout: open (%s) - drag the parts, the wheel sizes them, a right click puts one back", by);
}

void frame(HWND game, double k, int targetW, int targetH) {
    State& s = state();
    s.k = float(k);
    s.targetW = targetW;
    s.targetH = targetH;
    ++s.frameNo;  // the parts placed from now on are the next frame's
    s.shown = false;
    if (!s.on || !game || k <= 0 || targetW <= 0 || targetH <= 0) return;
    if (!inGameWorld()) {  // the login, the characters, a map's loading: nothing of it there
        close(s, "the game world left");
        return;
    }
    RECT client{};
    GetClientRect(game, &client);
    uapad::gamewindow::setBlocked(uapad::gamewindow::Blocker::HudEdit, game, &client, true);  // the world takes none
    const float scale = uistyle::partScale(k);
    s.bar = hudlayout::barLayout(targetW, scale);
    int cx = 0, cy = 0;
    const bool pointer = cursor(s, cx, cy);
    if (s.drag >= 0 && pointer) {  // the dragged part follows the pointer, snapped
        const hudlayout::Box& was = s.boxes[size_t(s.drag)];
        std::vector<hudlayout::Box> others;
        for (int i = 0; i < kParts; ++i)
            if (i != s.drag && known(s, i)) others.push_back(s.boxes[size_t(i)]);
        const hudlayout::Snapped at = hudlayout::snap(hudlayout::Box{cx - s.grabX, cy - s.grabY, was.w, was.h},
                                                      others, targetW, targetH, int(std::lround(10 * scale)));
        s.guideX = at.guideX;
        s.guideY = at.guideY;
        hudlayout::Place& p = s.layout[size_t(s.drag)];
        const hudlayout::Place next =
            hudlayout::placeOf(hudlayout::Box{at.x, at.y, was.w, was.h}, s.k, targetW, targetH, p.size);
        if (!p.moved || std::fabs(next.dx - p.dx) > 0.01f || std::fabs(next.dy - p.dy) > 0.01f ||
            next.anchor != p.anchor) {
            p = next;
            touch(s, Part(s.drag));
            if (!shownNow(s, s.drag))  // not shown: no part places its box - the frame follows the pointer
                s.boxes[size_t(s.drag)] = hudlayout::Box{at.x, at.y, was.w, was.h};
        }
    }
    s.barHover = pointer ? hudlayout::barHit(s.bar, float(cx - s.bar.x), float(cy - s.bar.y))
                         : hudlayout::BarPart::None;
    s.hover = s.drag >= 0 ? s.drag : s.barHover == hudlayout::BarPart::None && pointer ? partAt(s, cx, cy) : -1;
    // the bar
    const std::string barKey = std::to_string(s.bar.w) + ';' + std::to_string(int(s.barHover)) + ';' +
                               std::to_string(int(startup().lang));
    if (barKey != s.barPicture.key) {
        s.barPicture.key = barKey;
        hudlayout::paintBar(s.barPicture.picture, s.bar, s.barHover);
        const hudlayout::BarTexts t{say(settings::Text::HudEditTitle), say(settings::Text::HudEditHint),
                                    say(settings::Text::HudEditReset), say(settings::Text::HudEditDone)};
        drawWords(s.barPicture.picture, gdiWords(hudlayout::barWords(s.bar, t)));
        s.barPicture.upload = true;
    }
    // each part's frame
    for (int i = 0; i < kParts; ++i) {
        if (!known(s, i)) continue;
        const hudlayout::Box& b = s.boxes[size_t(i)];
        const hudlayout::Look look = i == s.drag    ? hudlayout::Look::Dragged
                                     : i == s.hover ? hudlayout::Look::Hover
                                                    : hudlayout::Look::Idle;
        const int percent = int(std::lround(s.layout[size_t(i)].size * 100));
        const bool live = shownNow(s, i);
        const std::string key = std::to_string(b.w) + 'x' + std::to_string(b.h) + ';' + std::to_string(int(look)) +
                                ';' + std::to_string(percent) + (live ? ";" : ";u") +
                                std::to_string(int(startup().lang));
        Drawn& d = s.frames[size_t(i)];
        if (key == d.key) continue;
        d.key = key;
        std::string name = say(kNames[i]);
        if (!live) name += " \xC2\xB7 " + say(settings::Text::HudEditUnseen);
        const std::vector<hudlayout::Words> words = hudlayout::frameWords(b.w, b.h, scale, name, percent);
        const float tag = words.empty() ? 0.0f
                                        : float(wordsWidth(utf8Wide(words[0].text), words[0].px, words[0].bold));
        hudlayout::paintFrame(d.picture, b.w, b.h, scale, look, tag);
        drawWords(d.picture, gdiWords(words));
        d.upload = true;
    }
    if (s.guide.picture.width == 0) {  // a gold pixel, stretched into the guides
        s.guide.picture.width = s.guide.picture.height = 1;
        s.guide.picture.px.assign(1, 0xFFE6CB8Au);
        s.guide.upload = true;
    }
    s.shown = true;
}

void draw(FrameRecord& record, gpu::Renderer& renderer) {
    State& s = state();
    if (!s.shown || s.k <= 0) return;
    const size_t world = record.firstInterface();  // the world dimmed under the interface
    const size_t before = record.commands().size();
    drawShade(record, 0, 0, s.targetW, s.targetH, s.k, 0.35f);
    const size_t added = record.commands().size() - before;
    if (added) record.moveLastBefore(added, world);
    for (int i = 0; i < kParts; ++i) {
        if (!known(s, i) || s.frames[size_t(i)].picture.width <= 0) continue;
        Drawn& d = s.frames[size_t(i)];
        const hudlayout::Box& b = s.boxes[size_t(i)];
        drawPart(record, renderer, d.picture, d.texture, d.texW, d.texH, d.upload, b.x, b.y, s.k,
                 shownNow(s, i) ? 1.0f : 0.6f);
    }
    if (s.drag >= 0) {
        const int thin = std::max(1, int(std::lround(s.k)));
        if (s.guideX >= 0)
            drawPartStretched(record, renderer, s.guide.picture, s.guide.texture, s.guide.texW, s.guide.texH,
                              s.guide.upload, s.guideX - thin / 2, 0, thin, s.targetH, s.k, 0.8f);
        if (s.guideY >= 0)
            drawPartStretched(record, renderer, s.guide.picture, s.guide.texture, s.guide.texW, s.guide.texH,
                              s.guide.upload, 0, s.guideY - thin / 2, s.targetW, thin, s.k, 0.8f);
    }
    drawPart(record, renderer, s.barPicture.picture, s.barPicture.texture, s.barPicture.texW, s.barPicture.texH,
             s.barPicture.upload, s.bar.x, s.bar.y, s.k);
}

bool mouse(HWND, UINT m, WPARAM wp, LPARAM, LRESULT& result) {
    State& s = state();
    if (!s.on) return false;
    const bool button = m == WM_LBUTTONDOWN || m == WM_LBUTTONUP || m == WM_LBUTTONDBLCLK || m == WM_RBUTTONDOWN ||
                        m == WM_RBUTTONUP || m == WM_RBUTTONDBLCLK || m == WM_MBUTTONDOWN || m == WM_MBUTTONUP;
    if (!button && m != WM_MOUSEWHEEL) return false;
    result = 0;
    int cx = 0, cy = 0;
    if (!cursor(s, cx, cy)) return true;
    const hudlayout::BarPart bar = hudlayout::barHit(s.bar, float(cx - s.bar.x), float(cy - s.bar.y));
    const int part = bar == hudlayout::BarPart::None ? partAt(s, cx, cy) : -1;
    if (m == WM_LBUTTONDOWN || m == WM_LBUTTONDBLCLK) {
        if (bar == hudlayout::BarPart::Done) {
            close(s, "«Готово»");
        } else if (bar == hudlayout::BarPart::Reset) {
            for (int i = 0; i < kParts; ++i)
                if (s.layout[size_t(i)].moved || std::fabs(s.layout[size_t(i)].size - 1) > 1e-3f) {
                    s.layout[size_t(i)] = hudlayout::Place{};
                    touch(s, Part(i));
                }
            save(s, Part::kCount, "all the parts back in their own places");
        } else if (part >= 0) {
            const hudlayout::Box& b = s.boxes[size_t(part)];
            s.drag = part;
            s.grabX = cx - b.x;
            s.grabY = cy - b.y;
        }
    } else if (m == WM_LBUTTONUP && s.drag >= 0) {
        const Part dragged = Part(s.drag);
        s.drag = -1;
        s.guideX = s.guideY = -1;
        save(s, dragged, "moved");
    } else if (m == WM_RBUTTONDOWN && part >= 0) {
        s.layout[size_t(part)] = hudlayout::Place{};
        touch(s, Part(part));
        save(s, Part(part), "back");
    } else if (m == WM_MOUSEWHEEL && part >= 0) {
        hudlayout::Place& p = s.layout[size_t(part)];
        const float size = hudlayout::stepped(p.size, GET_WHEEL_DELTA_WPARAM(wp) > 0 ? 1 : -1);
        if (std::fabs(size - p.size) > 1e-4f) {
            p.size = size;
            touch(s, Part(part));
            save(s, Part(part), "sized");
        }
    }
    return true;
}

bool key(UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    if (m != WM_KEYDOWN || (lp & (LPARAM(1) << 30))) return s.on && m == WM_KEYUP && wp == VK_ESCAPE;
    if (wp == 'L' && GetKeyState(VK_CONTROL) < 0 && GetKeyState(VK_SHIFT) < 0) {
        if (s.on) close(s, "Ctrl+Shift+L");
        else open(nullptr, "Ctrl+Shift+L");
        return true;
    }
    if (s.on && wp == VK_ESCAPE) {
        close(s, "Esc");
        return true;
    }
    return false;
}

}  // namespace uagfx::win::hudedit
