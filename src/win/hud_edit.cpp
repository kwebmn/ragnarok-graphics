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
using uapad::gamewindow::Blocker;
constexpr int kParts = hudlayout::kParts;
// The order the parts are drawn in (win/context): the last drawn is the one under the pointer first
constexpr Part kOnTop[] = {Part::Chat, Part::Hotbar, Part::Party, Part::Target, Part::Character,
                           Part::Buffs, Part::Tracker, Part::Minimap};
const settings::Text kNames[kParts] = {settings::Text::HudPartCharacter, settings::Text::HudPartMinimap,
                                       settings::Text::HudPartBuffs,     settings::Text::HudPartTracker,
                                       settings::Text::HudPartHotbar,    settings::Text::HudPartTarget,
                                       settings::Text::HudPartParty,     settings::Text::HudPartChat};
// 3.0.11-beta.77: each part's own blocked rectangle - its solid part, what takes its clicks
const Blocker kSolid[kParts] = {Blocker::Character, Blocker::Minimap, Blocker::Buffs, Blocker::Tracker,
                                Blocker::Hotbar,    Blocker::Target,  Blocker::Party, Blocker::Chat};
// The windows over the parts: a press there is theirs
const Blocker kWindows[] = {Blocker::Panel, Blocker::Card, Blocker::Reference, Blocker::BigMap, Blocker::Items,
                            Blocker::Stats};
constexpr float kMoveGame = 6;   // game pixels the pointer moves before a held press drags its part
constexpr float kGripGame = 14;  // the grip's side, off the part's bottom right corner

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
    std::array<hudlayout::Box, kParts> boxes{};  // the parts' pictures
    std::array<hudlayout::Box, kParts> solid{};  // their solid parts (3.0.11-beta.77)
    std::array<bool, kParts> solidKnown{};
    std::array<int, kParts> seen{};  // the frame each part was last placed in
    int frameNo = 1;
    bool on = false, shown = false, locked = false, blocking = false;
    float k = 0;
    int targetW = 0, targetH = 0;
    // a drag (framed, or straight from a held press): the part, its solid's grab point and offset from its picture
    int drag = -1;
    bool straight = false;
    int grabX = 0, grabY = 0, solidDx = 0, solidDy = 0;
    int guideX = -1, guideY = -1;
    hudlayout::Box dropAt;  // the solid where it would be let go
    // a press held on a part (3.0.11-beta.77): the part's after all unless the pointer moves
    bool held = false;
    int heldPart = -1, pressX = 0, pressY = 0;
    UINT heldM = 0;
    WPARAM heldWp = 0;
    LPARAM heldLp = 0;
    // sizing by the grip: the picture's top left kept, the size by the pointer's distance from it
    int sizing = -1, topX = 0, topY = 0, picW = 0, picH = 0;
    float sizeFrom = 1, reachFrom = 1;
    int gripPart = -1;
    bool gripHover = false;
    int hover = -1;
    hudlayout::BarPart barHover = hudlayout::BarPart::None;
    hudlayout::Bar bar;
    Drawn barPicture, guide, grip;
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
    const bool ok = writeFileAtomic(layout::join(gameDir(), layout::kHudLayout), hudlayout::text(s.layout));
    if (int(part) < kParts) {
        const hudlayout::Place& p = s.layout[size_t(part)];
        const std::string where = p.moved ? "at the screen's point " + std::to_string(p.anchor) + " " +
                                                std::to_string(int(p.dx)) + ", " + std::to_string(int(p.dy))
                                          : "its own place";
        LOG_INFO("interface layout: %s %s - %s, %d %%%s", hudlayout::keyOf(part), what, where.c_str(),
                 int(std::lround(p.size * 100)), ok ? "" : " - NOT saved");
    } else {
        LOG_INFO("interface layout: %s%s", what, ok ? "" : " - NOT saved");
    }
}

void touch(State& s, Part part) { ++s.stamps[size_t(part)]; }

bool shownNow(const State& s, int i) { return s.seen[size_t(i)] >= s.frameNo - 1; }
// Shown before, not now (a target's card without a target): its frame where it was, faint - still moved
bool known(const State& s, int i) { return s.seen[size_t(i)] > 0; }
// What of a part takes the pointer: its solid part while it shows (3.0.11-beta.77), else its picture
hudlayout::Box solidOf(const State& s, int i) {
    return s.solidKnown[size_t(i)] && shownNow(s, i) ? s.solid[size_t(i)] : s.boxes[size_t(i)];
}
bool inside(const hudlayout::Box& b, int x, int y) { return x >= b.x && y >= b.y && x < b.x + b.w && y < b.y + b.h; }

// 3.0.11-beta.79 (the owner: «когда тяну мышкой интерфейс то окошко скачет сильно»): a part standing by another by
// default (not moved by the player) goes with it - the one dragged is neither snapped to it nor kept clear of it (it
// moved with it each frame and pulled it on): the buffs under the minimap, the tracker by the buffs (else the minimap),
// the party under the character block, the hotbar past the chat
int leaderOf(const State& s, int part) {
    switch (Part(part)) {
        case Part::Buffs: return int(Part::Minimap);
        case Part::Tracker: return shownNow(s, int(Part::Buffs)) ? int(Part::Buffs) : int(Part::Minimap);
        case Part::Party: return int(Part::Character);
        case Part::Hotbar: return int(Part::Chat);
        default: return -1;
    }
}

bool follows(const State& s, int part, int leader) {
    for (int p = part, steps = 0; p >= 0 && steps < 4; ++steps) {
        if (s.layout[size_t(p)].moved) return false;
        const int next = leaderOf(s, p);
        if (next == leader) return true;
        p = next;
    }
    return false;
}

int partAt(const State& s, int x, int y, bool liveOnly) {
    for (const bool live : {true, false})
        for (const Part p : kOnTop) {
            const int i = int(p);
            if ((!live && liveOnly) || !known(s, i) || shownNow(s, i) != live) continue;
            if (inside(solidOf(s, i), x, y)) return i;
        }
    return -1;
}

hudlayout::Box gripOf(const State& s, int i) {
    const hudlayout::Box b = solidOf(s, i);
    const int side = std::max(8, int(std::lround(kGripGame * s.k)));
    return hudlayout::Box{b.x + b.w - side / 3, b.y + b.h - side / 3, side, side};
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

bool toTarget(const State& s, HWND game, LPARAM lp, int& x, int& y) {
    RECT client{};
    if (!game || !GetClientRect(game, &client) || client.right <= 0 || client.bottom <= 0 || s.targetW <= 0)
        return false;
    x = int(int64_t(GET_X_LPARAM(lp)) * s.targetW / client.right);
    y = int(int64_t(GET_Y_LPARAM(lp)) * s.targetH / client.bottom);
    return true;
}

bool buttonDown() {  // the logical left button (the physical right one when Windows swaps them)
    return GetAsyncKeyState(GetSystemMetrics(SM_SWAPBUTTON) ? VK_RBUTTON : VK_LBUTTON) < 0;
}

void block(State& s, HWND game, bool all) {  // the world takes no clicks while the parts are framed or dragged
    if (all) {
        RECT client{};
        if (game && GetClientRect(game, &client)) uapad::gamewindow::setBlocked(Blocker::HudEdit, game, &client, true);
    } else if (s.blocking) {
        uapad::gamewindow::setBlocked(Blocker::HudEdit, nullptr, nullptr, false);
    }
    s.blocking = all;
}

void close(State& s, const char* by) {
    if (!s.on) return;
    s.on = false;
    s.drag = -1;
    LOG_INFO("interface layout: closed (%s)", by);
}

// A drag let go: clear of the other parts (3.0.11-beta.77: «налазят одно на другое и мешают»), then kept
void drop(State& s, const char* what) {
    const int i = s.drag;
    s.drag = -1;
    s.guideX = s.guideY = -1;
    if (i < 0) return;
    std::vector<hudlayout::Box> others;
    for (int j = 0; j < kParts; ++j)
        if (j != i && shownNow(s, j) && !follows(s, j, i)) others.push_back(solidOf(s, j));
    const hudlayout::Box clear = hudlayout::clearOf(s.dropAt, others, s.targetW, s.targetH,
                                                    int(std::lround(4 * s.k)));
    hudlayout::Place& p = s.layout[size_t(i)];
    if (clear.x != s.dropAt.x || clear.y != s.dropAt.y) {
        const hudlayout::Box& was = s.boxes[size_t(i)];
        p = hudlayout::placeOf(hudlayout::Box{clear.x - s.solidDx, clear.y - s.solidDy, was.w, was.h}, s.k, s.targetW,
                               s.targetH, p.size);
        touch(s, Part(i));
        LOG_INFO("interface layout: %s let go over another part - moved %d, %d clear of it", hudlayout::keyOf(Part(i)),
                 clear.x - s.dropAt.x, clear.y - s.dropAt.y);
    }
    save(s, Part(i), what);
}

void startDrag(State& s, int part, int x, int y, bool straight) {
    const hudlayout::Box solid = solidOf(s, part), picture = s.boxes[size_t(part)];
    s.drag = part;
    s.straight = straight;
    s.grabX = x - solid.x;
    s.grabY = y - solid.y;
    s.solidDx = solid.x - picture.x;
    s.solidDy = solid.y - picture.y;
    s.dropAt = solid;
}

// The dragged part follows the pointer, snapped to the screen and the other parts
void follow(State& s, int cx, int cy) {
    const hudlayout::Box was = solidOf(s, s.drag);
    std::vector<hudlayout::Box> others;
    for (int i = 0; i < kParts; ++i)
        if (i != s.drag && (shownNow(s, i) || s.on) && known(s, i) && !follows(s, i, s.drag))
            others.push_back(solidOf(s, i));
    // 3.0.11-beta.79: 6 of the game's pixels (it was 10 of the parts' scaled ones - 23 on a 4K screen, a jump)
    const hudlayout::Snapped at = hudlayout::snap(hudlayout::Box{cx - s.grabX, cy - s.grabY, was.w, was.h}, others,
                                                  s.targetW, s.targetH, std::max(2, int(std::lround(6 * s.k))));
    s.guideX = at.guideX;
    s.guideY = at.guideY;
    s.dropAt = hudlayout::Box{at.x, at.y, was.w, was.h};
    hudlayout::Place& p = s.layout[size_t(s.drag)];
    const hudlayout::Box& picture = s.boxes[size_t(s.drag)];
    const hudlayout::Place next = hudlayout::placeOf(
        hudlayout::Box{at.x - s.solidDx, at.y - s.solidDy, picture.w, picture.h}, s.k, s.targetW, s.targetH, p.size);
    if (!p.moved || std::fabs(next.dx - p.dx) > 0.01f || std::fabs(next.dy - p.dy) > 0.01f ||
        next.anchor != p.anchor) {
        p = next;
        touch(s, Part(s.drag));
        if (!shownNow(s, s.drag)) {  // not shown: no part places its box - the frame follows the pointer
            s.boxes[size_t(s.drag)] = hudlayout::Box{at.x - s.solidDx, at.y - s.solidDy, picture.w, picture.h};
            s.solid[size_t(s.drag)] = s.dropAt;
        }
    }
}

// Sizing: the pointer's distance from the picture's top left against the distance it began at
void resize(State& s, int cx, int cy) {
    const float reach = std::hypot(float(cx - s.topX), float(cy - s.topY));
    hudlayout::Place& p = s.layout[size_t(s.sizing)];
    const float size = hudlayout::stepped(s.sizeFrom * reach / std::max(1.0f, s.reachFrom), 0);
    if (std::fabs(size - p.size) < 1e-4f) return;
    const float grow = size / std::max(0.01f, s.sizeFrom);
    p = hudlayout::placeOf(hudlayout::Box{s.topX, s.topY, int(std::lround(float(s.picW) * grow)),
                                          int(std::lround(float(s.picH) * grow))},
                           s.k, s.targetW, s.targetH, size);
    touch(s, Part(s.sizing));
}

void paintFrame(State& s, int i, float scale) {
    const hudlayout::Box b = solidOf(s, i);
    const hudlayout::Look look = i == s.drag    ? hudlayout::Look::Dragged
                                 : i == s.hover ? hudlayout::Look::Hover
                                                : hudlayout::Look::Idle;
    const int percent = int(std::lround(s.layout[size_t(i)].size * 100));
    const bool live = shownNow(s, i);
    const std::string key = std::to_string(b.w) + 'x' + std::to_string(b.h) + ';' + std::to_string(int(look)) + ';' +
                            std::to_string(percent) + (live ? ";" : ";u") + std::to_string(int(startup().lang));
    Drawn& d = s.frames[size_t(i)];
    if (key == d.key) return;
    d.key = key;
    std::string name = say(kNames[i]);
    if (!live) name += " \xC2\xB7 " + say(settings::Text::HudEditUnseen);
    const std::vector<hudlayout::Words> words = hudlayout::frameWords(b.w, b.h, scale, name, percent);
    const float tag = words.empty() ? 0.0f : float(wordsWidth(utf8Wide(words[0].text), words[0].px, words[0].bold));
    hudlayout::paintFrame(d.picture, b.w, b.h, scale, look, tag);
    drawWords(d.picture, gdiWords(words));
    d.upload = true;
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

bool movedOf(Part part) {
    State& s = state();
    readLayout(s);
    return int(part) < kParts && s.layout[size_t(part)].moved;
}

void setLocked(bool locked) {
    State& s = state();
    if (locked == s.locked) return;
    s.locked = locked;
    LOG_INFO("interface layout: [interface] lock_layout = %d - the parts %s", locked ? 1 : 0,
             locked ? "stay where they are" : "move with the mouse");
}

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
    if (!game || k <= 0 || targetW <= 0 || targetH <= 0 || !inGameWorld()) {  // the login, a map's loading
        close(s, "the game world left");
        s.held = false;
        s.drag = s.sizing = s.gripPart = -1;
        block(s, game, false);
        return;
    }
    // the parts' solid parts this frame (3.0.11-beta.77): their blocked rectangles in the picture's pixels
    RECT client{};
    GetClientRect(game, &client);
    for (int i = 0; i < kParts; ++i) {
        RECT r{};
        s.solidKnown[size_t(i)] = client.right > 0 && client.bottom > 0 && shownNow(s, i) &&
                                  uapad::gamewindow::blockerRect(kSolid[i], r);
        if (!s.solidKnown[size_t(i)]) continue;
        RECT lines{};  // the buffs' further columns with them
        if (kSolid[i] == Blocker::Buffs && uapad::gamewindow::blockerRect(Blocker::BuffsLines, lines)) {
            r.left = std::min(r.left, lines.left);
            r.top = std::min(r.top, lines.top);
            r.right = std::max(r.right, lines.right);
            r.bottom = std::max(r.bottom, lines.bottom);
        }
        const int x0 = int(int64_t(r.left) * targetW / client.right);
        const int y0 = int(int64_t(r.top) * targetH / client.bottom);
        const int x1 = int(int64_t(r.right) * targetW / client.right);
        const int y1 = int(int64_t(r.bottom) * targetH / client.bottom);
        s.solid[size_t(i)] = hudlayout::Box{x0, y0, std::max(1, x1 - x0), std::max(1, y1 - y0)};
    }
    const float scale = uistyle::partScale(k);
    int cx = 0, cy = 0;
    const bool pointer = cursor(s, cx, cy);
    const bool down = buttonDown();
    // 3.0.11-beta.77: a held press that moved - its part dragged; one let go outside the window - forgotten
    if (s.held && !down) s.held = false;
    if (s.held && pointer &&
        std::hypot(float(cx - s.pressX), float(cy - s.pressY)) >= kMoveGame * s.k && s.heldPart >= 0) {
        s.held = false;
        startDrag(s, s.heldPart, s.pressX, s.pressY, true);
        LOG_INFO("interface layout: %s dragged with the mouse", hudlayout::keyOf(Part(s.drag)));
    }
    if (s.drag >= 0 && s.straight && !down) drop(s, "dragged");  // its release went elsewhere
    if (s.sizing >= 0 && !down) {
        save(s, Part(s.sizing), "sized");
        s.sizing = -1;
    }
    if (s.drag >= 0 && pointer) follow(s, cx, cy);
    if (s.sizing >= 0 && pointer) resize(s, cx, cy);
    // the grip at the corner of the part under the pointer
    s.gripPart = -1;
    s.gripHover = false;
    if (!s.locked && !s.on && s.drag < 0 && s.sizing < 0 && !s.held && pointer) {
        for (const Part p : kOnTop) {
            const int i = int(p);
            if (!shownNow(s, i)) continue;
            const hudlayout::Box b = solidOf(s, i), g = gripOf(s, i);
            if (inside(b, cx, cy) || inside(g, cx, cy)) {
                s.gripPart = i;
                s.gripHover = inside(g, cx, cy);
                break;
            }
        }
    }
    if (s.sizing >= 0) s.gripPart = s.sizing, s.gripHover = true;
    block(s, game, s.on || s.drag >= 0 || s.sizing >= 0);
    if (s.on) {
        s.bar = hudlayout::barLayout(targetW, scale);
        s.barHover = pointer ? hudlayout::barHit(s.bar, float(cx - s.bar.x), float(cy - s.bar.y))
                             : hudlayout::BarPart::None;
        s.hover = s.drag >= 0                                                      ? s.drag
                  : s.barHover == hudlayout::BarPart::None && pointer ? partAt(s, cx, cy, false)
                                                                      : -1;
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
        for (int i = 0; i < kParts; ++i)
            if (known(s, i)) paintFrame(s, i, scale);
    } else {
        s.hover = s.drag >= 0 ? s.drag : s.sizing;
        if (s.hover >= 0) paintFrame(s, s.hover, scale);
    }
    if (s.gripPart >= 0) {  // a small handle off the corner
        const int side = gripOf(s, s.gripPart).w;
        const std::string key = std::to_string(side) + (s.gripHover ? "h" : "");
        if (key != s.grip.key) {
            s.grip.key = key;
            hudlayout::paintGrip(s.grip.picture, side, s.k, s.gripHover);
            s.grip.upload = true;
        }
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
    if (s.on) {  // the world dimmed under the interface
        const size_t world = record.firstInterface();
        const size_t before = record.commands().size();
        drawShade(record, 0, 0, s.targetW, s.targetH, s.k, 0.35f);
        const size_t added = record.commands().size() - before;
        if (added) record.moveLastBefore(added, world);
    }
    for (int i = 0; i < kParts; ++i) {
        if (!(s.on ? known(s, i) : i == s.hover) || s.frames[size_t(i)].picture.width <= 0) continue;
        Drawn& d = s.frames[size_t(i)];
        const hudlayout::Box b = solidOf(s, i);
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
    if (s.gripPart >= 0 && s.grip.picture.width > 0) {
        const hudlayout::Box g = gripOf(s, s.gripPart);
        drawPart(record, renderer, s.grip.picture, s.grip.texture, s.grip.texW, s.grip.texH, s.grip.upload, g.x, g.y,
                 s.k);
    }
    if (s.on)
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
    const int part = bar == hudlayout::BarPart::None ? partAt(s, cx, cy, false) : -1;
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
            startDrag(s, part, cx, cy, false);
        }
    } else if (m == WM_LBUTTONUP && s.drag >= 0) {
        drop(s, "moved");
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

Gate gate(HWND game, UINT m, WPARAM wp, LPARAM lp) {
    State& s = state();
    if (s.on) return Gate::Pass;  // framed: mouse() has all of it
    if (m == WM_LBUTTONUP) {
        if (s.drag >= 0 && s.straight) {
            drop(s, "dragged");
            return Gate::Take;
        }
        if (s.sizing >= 0) {
            save(s, Part(s.sizing), "sized");
            s.sizing = -1;
            return Gate::Take;
        }
        if (s.held) {
            s.held = false;
            return Gate::PressFirst;
        }
        return Gate::Pass;
    }
    if ((m != WM_LBUTTONDOWN && m != WM_LBUTTONDBLCLK) || s.locked || s.k <= 0 || s.drag >= 0) return Gate::Pass;
    int x = 0, y = 0;
    if (!toTarget(s, game, lp, x, y)) return Gate::Pass;
    if (s.gripPart >= 0 && inside(gripOf(s, s.gripPart), x, y)) {  // the grip: the part's size
        const int i = s.gripPart;
        if (m == WM_LBUTTONDBLCLK) {  // twice: its own place and size
            s.layout[size_t(i)] = hudlayout::Place{};
            touch(s, Part(i));
            save(s, Part(i), "back (the grip twice)");
            return Gate::Take;
        }
        const hudlayout::Box& picture = s.boxes[size_t(i)];
        s.sizing = i;
        s.sizeFrom = s.layout[size_t(i)].size;
        s.topX = picture.x;
        s.topY = picture.y;
        s.picW = picture.w;
        s.picH = picture.h;
        s.reachFrom = std::hypot(float(x - s.topX), float(y - s.topY));
        return Gate::Take;
    }
    for (const Blocker w : kWindows) {  // a window over the parts: its own
        RECT r{};
        if (uapad::gamewindow::blockerRect(w, r) && GET_X_LPARAM(lp) >= r.left && GET_X_LPARAM(lp) < r.right &&
            GET_Y_LPARAM(lp) >= r.top && GET_Y_LPARAM(lp) < r.bottom)
            return Gate::Pass;
    }
    const int part = partAt(s, x, y, true);
    if (part < 0) return Gate::Pass;
    s.held = true;
    s.heldPart = part;
    s.heldM = m;
    s.heldWp = wp;
    s.heldLp = lp;
    s.pressX = x;
    s.pressY = y;
    return Gate::Take;
}

void heldPress(UINT& m, WPARAM& wp, LPARAM& lp) {
    const State& s = state();
    m = s.heldM;
    wp = s.heldWp;
    lp = s.heldLp;
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
