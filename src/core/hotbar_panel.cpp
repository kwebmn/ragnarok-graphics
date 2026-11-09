#include "core/hotbar_panel.hpp"

#include <algorithm>
#include <cmath>

#include "core/settings/strings.hpp"
#include "core/pad_glyphs.hpp"
#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::hotbar {
namespace {

// The concept's sizes at scale 1 (v17_hotbar): a cell 48 (its icon 32), the battle mode's 44 (30), 4 apart; the bar 6
// in; the row's chip 34 wide; the battle mode's button 28 (3.0.11-beta.147: the pencil's place); a tip 42 high over the
// bar
constexpr float kCell = 48, kCellBattle = 44, kGap = 4, kPad = 6, kRowW = 34, kLabelW = 18, kToggle = 28;
constexpr float kHeaderH = 22, kTipH = 42;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000;
constexpr uint32_t kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xECEFF5, kMuted = 0x9AA3B5, kRed = 0xC0392B;
// 3.0.11-beta.45: the «SP» plate (the SP bar's blue), the tips' lines of no SP and of «не тут» (the concept's red for
// small words, 4.9:1)
constexpr uint32_t kSpBlue = 0x3D7EEB, kSpWords = 0x9CC3FF, kNotHereWords = 0xFF9A8F;
// 3.0.11-beta.125: the ammo running low - the character block's amber and red (core/char_block)
constexpr uint32_t kAmmoWarn = 0xF0A050, kAmmoOut = 0xFF5A4E;
constexpr int kAmmoLow = 50, kAmmoVeryLow = 10;
constexpr float kAmmoH = 26, kAmmoIcon = 18, kAmmoPx = 12.5f;

const char* const kBattleKeys[3][9] = {{"Q", "W", "E", "R", "T", "Y", "U", "I", "O"},
                                       {"A", "S", "D", "F", "G", "H", "J", "K", "L"},
                                       {"Z", "X", "C", "V", "B", "N", "M", ",", "."}};

float cellSide(Mode m) { return m == Mode::Battle ? kCellBattle : kCell; }

// A premultiplied picture centred in a box, its alpha times `k` (`grey`: its colours as their brightness)
void put(panel::Picture& out, const panel::Picture& pic, const panel::Box& box, float k, bool grey = false) {
    const int ox = int(std::lround(box.x + (box.w - float(pic.width)) / 2));
    const int oy = int(std::lround(box.y + (box.h - float(pic.height)) / 2));
    if (out.scene) {  // 3.0.18-beta.12: recorded for uaUI
        panel::recordImage(out, pic, float(ox), float(oy), k, 0.0f, grey);
        return;
    }
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
            if (grey) {
                const uint32_t v = (((c >> 16) & 0xFF) * 77 + ((c >> 8) & 0xFF) * 150 + (c & 0xFF) * 29) >> 8;
                c = (c & 0xFF000000u) | v << 16 | v << 8 | v;
            }
            if (k < 1) {
                const auto ch = [&](int sh) { return uint32_t(float((c >> sh) & 0xFF) * k + 0.5f); };
                c = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
            }
            if (!(c >> 24)) continue;
            uint32_t& d = out.px[size_t(ty) * size_t(out.width) + size_t(tx)];
            const uint32_t keep = 255 - (c >> 24);
            const auto ch = [&](int sh) {
                return std::min<uint32_t>(255, ((c >> sh) & 0xFF) + ((d >> sh) & 0xFF) * keep / 255);
            };
            d = ch(24) << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
        }
}

// A skill's level at the cell's bottom left
panel::Box levelBox(const panel::Box& cell, int level, float s) {
    const float w = panel::wordsWidth(std::to_string(level), 11 * s, true) + 6 * s;
    return panel::Box{cell.x + 2 * s, cell.y + cell.h - 15 * s, w, 13 * s};
}

bool noSp(const Cell& c) { return c.spNeed > 0 && c.spHave < c.spNeed; }
constexpr uint32_t kLackRed = 0xC8443A, kReadyBlue = 0x4DA3FF;  // 3.0.16-beta.13

// 3.0.16-beta.13: the plate of what it lacks - the top right, as the «SP» one
panel::Box lackBox(const panel::Box& cell, const std::string& text, float s) {
    const float w = panel::wordsWidth(text, 11 * s, true) + 6 * s;
    return panel::Box{cell.x + cell.w - w - 2 * s, cell.y + 2 * s, w, 13 * s};
}

// 3.0.16-beta.22: a ground skill's time left - its plate in the cell's middle
panel::Box timerPlate(const panel::Box& cell, const std::string& text, float s) {
    const float w = panel::wordsWidth(text, 12 * s, true) + 8 * s, h = 15 * s;
    return panel::Box{cell.x + (cell.w - w) / 2, cell.y + (cell.h - h) / 2, w, h};
}

// 3.0.11-beta.143: a ground skill's own count's plate - the bottom right, as an item's count
panel::Box standingBox(const panel::Box& cell, const std::string& text, float s) {
    const float w = panel::wordsWidth(text, 11 * s, true) + 6 * s;
    return panel::Box{cell.x + cell.w - w - 2 * s, cell.y + cell.h - 15 * s, w, 13 * s};
}

// The cell as shown: pressed, 1 px lower
panel::Box shown(const Layout& L, size_t i, const Show& show) {
    panel::Box b = L.cells[i];
    if (int(i) == show.pressed) b.y += 1 * L.s;
    return b;
}

// The «SP» plate at the cell's top right (no SP for it)
panel::Box spBox(const panel::Box& cell, float s) {
    const float w = panel::wordsWidth("SP", 11 * s, true) + 6 * s;
    return panel::Box{cell.x + cell.w - w - 2 * s, cell.y + 2 * s, w, 13 * s};
}

// The hovered cell's tip: its words (the second line why it cannot be used, else how to use it), its box over the bar
struct TipLine {
    std::string text;
    uint32_t rgb = kMuted;
};
std::vector<TipLine> tipLines(const Cell& c, const Texts& t) {
    if (c.name.empty()) return {};
    std::string first = c.name;
    if (c.level > 0) first += " \xC2\xB7 Lv " + std::to_string(c.level);
    if (c.count >= 0) first += " \xC2\xB7 \xC3\x97" + std::to_string(c.count);
    if (c.unlearned) return {{first, kText}, {t.unlearned, kNotHereWords}};  // 3.0.11-beta.78
    const size_t why = size_t(c.notHere);
    if (c.notHere != NotHere::No && why < t.reasons.size())
        return {{first, kText}, {settings::fmt(t.notHere, {t.reasons[why]}), kNotHereWords}};
    if (noSp(c))
        return {{first, kText},
                {settings::fmt(t.noSp, {std::to_string(c.spNeed), std::to_string(c.spHave)}), kSpWords}};
    if (!c.lackWhy.empty()) return {{first, kText}, {c.lackWhy, kLackRed}};  // 3.0.16-beta.13
    if (c.levelMost > 1 && !t.wheel.empty())  // 3.0.11-beta.289: the wheel sets its level
        return {{first, kText},
                {t.use, kMuted},
                {settings::fmt(t.wheel, {std::to_string(c.levelMost)}), kMuted}};
    return {{first, kText}, {t.use, kMuted}};
}

panel::Box tipBox(const Layout& L, int hover, const std::vector<TipLine>& lines) {
    if (hover < 0 || hover >= int(L.cells.size()) || lines.empty()) return panel::Box{};
    float w = 0;
    for (size_t i = 0; i < lines.size(); ++i)
        w = std::max(w, panel::wordsWidth(lines[i].text, (i ? 11.5f : 13) * L.s, i == 0));
    w += 20 * L.s;
    const panel::Box& c = L.cells[size_t(hover)];
    const float x = std::clamp(c.x + c.w / 2 - w / 2, 0.0f, std::max(0.0f, float(L.w) - w));
    return panel::Box{x, L.below ? L.tipRoom.y : L.tipRoom.y + L.tipRoom.h - kTipH * L.s, w, kTipH * L.s};
}

// 3.0.11-beta.147: the battle mode button's tip over it - what a click does
const std::string& toggleText(const Layout& L, const Texts& t) {
    return L.mode == Mode::Battle ? t.battleOff : t.battleOn;
}
panel::Box toggleTip(const Layout& L, const Texts& t) {
    const float w = panel::wordsWidth(toggleText(L, t), 12 * L.s, false) + 20 * L.s;
    const float x = std::clamp(L.toggle.x + L.toggle.w / 2 - w / 2, 0.0f, std::max(0.0f, float(L.w) - w));
    return panel::Box{x, L.below ? L.tipRoom.y + 4 * L.s : L.tipRoom.y + L.tipRoom.h - 26 * L.s, w, 22 * L.s};
}

bool overlap(const panel::Box& a, const panel::Box& b) {
    return a.w > 0 && b.w > 0 && a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

// 3.0.11-beta.125: the ammo pill's words - the warning's before the count - and their colour by how low it is
std::string ammoText(const Ammo& a) {
    const std::string n = std::to_string(std::max(0, a.count));
    return a.warning.empty() ? n : a.warning + " \xC2\xB7 " + n;
}
uint32_t ammoRgb(int count) { return count <= kAmmoVeryLow ? kAmmoOut : count < kAmmoLow ? kAmmoWarn : kText; }

// The pill where it would be, a tip over it or not
panel::Box ammoPlace(const Layout& L, const Ammo& a) {
    if (a.count < 0) return panel::Box{};
    const float s = L.s;
    const float w = 8 * s + kAmmoIcon * s + 6 * s + panel::wordsWidth(ammoText(a), kAmmoPx * s, true) + 10 * s;
    const float x = std::clamp(L.glass.x, 0.0f, std::max(0.0f, float(L.w) - w));
    const float y = L.below ? L.glass.y + L.glass.h + 6 * s : L.glass.y - kAmmoH * s - 6 * s;  // 3.0.12-beta.2
    return panel::Box{x, y, w, kAmmoH * s};
}

// 3.0.16-beta.8: the supplies' pills in a row after the ammo's (or where it would be)
constexpr float kDot = 7, kDotGap = 3;
std::string supplyText(const Supply& p) { return std::to_string(std::max(0, p.count)); }
float supplyWidth(const Supply& p, float s) {
    if (p.dots > 0) return 10 * s + float(p.dots) * kDot * s + float(p.dots - 1) * kDotGap * s + 10 * s;
    return 8 * s + kAmmoIcon * s + 6 * s + panel::wordsWidth(supplyText(p), kAmmoPx * s, true) + 10 * s;
}
std::vector<panel::Box> supplyPlaces(const Layout& L, const Show& show) {
    std::vector<panel::Box> out;
    if (show.supplies.empty()) return out;
    const float s = L.s;
    const panel::Box a = ammoPlace(L, show.ammo);
    float x = a.w > 0 ? a.x + a.w + 6 * s : L.glass.x;
    const float y = L.below ? L.glass.y + L.glass.h + 6 * s : L.glass.y - kAmmoH * s - 6 * s;
    for (const Supply& p : show.supplies) {
        const float w = supplyWidth(p, s);
        out.push_back(panel::Box{x, y, w, kAmmoH * s});
        x += w + 6 * s;
    }
    return out;
}
uint32_t supplyRgb(const Supply& p) {
    return p.dots > 0 ? kText : p.count <= 0 ? kAmmoOut : p.count <= kAmmoVeryLow ? kAmmoWarn : kText;
}

// Shown: not under the hovered cell's tip nor the battle mode button's
panel::Box ammoShown(const Layout& L, const Show& show, const panel::Box& tip, const Texts& t) {
    const panel::Box b = ammoPlace(L, show.ammo);
    return overlap(b, tip) || (show.hoverToggle && overlap(b, toggleTip(L, t))) ? panel::Box{} : b;
}

}  // namespace

panel::Box timerBox(const panel::Box& cell, const std::string& text, float s) { return timerPlate(cell, text, s); }

int iconSide(Mode mode, float scale) { return int(std::lround((mode == Mode::Battle ? 30 : 32) * scale)); }

int gameRow(int shownRow) { return shownRow == 0 ? 1 : shownRow == 1 ? 2 : 0; }

const char* battleKey(int shownRow, int cell) {
    return shownRow >= 0 && shownRow < 3 && cell >= 0 && cell < 9 ? kBattleKeys[shownRow][cell] : "";
}

int ammoIconSide(float scale) { return int(std::lround(kAmmoIcon * (scale > 0 ? scale : 1.0f))); }

int ammoLevel(int count) {
    return count < 0 ? 0 : count == 0 ? 3 : count <= kAmmoVeryLow ? 2 : count < kAmmoLow ? 1 : 0;
}

int AmmoWarn::now(int count, uint64_t nowMs) {
    fresh = 0;
    if (count < 0) return 0;  // 3.0.11-beta.129: not known a while - the level kept
    const int l = ammoLevel(count);
    if (l > level) {  // fell lower: told
        warned = l;
        since = nowMs;
        fresh = l;
    }
    level = l;
    if (warned > l) warned = 0;  // more again
    if (!warned) return 0;
    return warned == 3 || nowMs - since < kShowMs ? warned : 0;
}

bool Settle::settled(bool over, uint64_t nowMs, uint64_t afterMs) {
    if (!over) {
        since = 0;
        return false;
    }
    if (!since) since = nowMs ? nowMs : 1;
    return nowMs - since >= afterMs;
}

Layout layout(Mode mode, int centreX, int bottom, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.mode = mode;
    const float pad = kPad * s, tip = kTipH * s + 6 * s;
    const float side = cellSide(mode) * s, gap = kGap * s;
    const float cellsW = 9 * side + 8 * gap;
    const bool battle = mode == Mode::Battle;
    const float leftW = (battle ? kLabelW : kRowW) * s;
    const float w = pad + leftW + pad + cellsW + pad + kToggle * s + pad;
    const float top = tip + pad + (battle ? kHeaderH * s + gap : 0.0f);
    const int rows = battle ? 3 : 1;
    const float h = (top - tip) + float(rows) * side + float(rows - 1) * gap + pad;
    L.glass = panel::Box{0, tip, w, h};
    const float x0 = pad + leftW + pad;
    for (int r = 0; r < rows; ++r) {
        const float y = top + float(r) * (side + gap);
        for (int c = 0; c < 9; ++c) L.cells.push_back(panel::Box{x0 + float(c) * (side + gap), y, side, side});
        if (battle) L.rowLabels.push_back(panel::Box{pad, y, leftW, side});
    }
    if (battle) L.header = panel::Box{x0, tip + pad, cellsW, kHeaderH * s};
    else L.row = panel::Box{pad, top, leftW, side};
    L.toggle = panel::Box{x0 + cellsW + pad, top + ((battle ? 3 * side + 2 * gap : side) - kToggle * s) / 2,
                          kToggle * s, kToggle * s};
    L.tipRoom = panel::Box{0, 0, L.glass.w, tip - 6 * s};
    L.w = int(std::ceil(std::max(L.glass.w, 300 * s)));
    // the glass centred in the picture
    const float shift = (float(L.w) - L.glass.w) / 2;
    for (panel::Box* b : {&L.glass, &L.row, &L.header, &L.toggle}) b->x += shift;
    for (panel::Box& b : L.cells) b.x += shift;
    for (panel::Box& b : L.rowLabels) b.x += shift;
    L.tipRoom.w = float(L.w);
    L.h = int(std::ceil(L.glass.y + L.glass.h));
    L.x = centreX - L.w / 2;
    L.y = bottom - L.h;
    return L;
}

void flip(Layout& L) {
    if (L.below) return;
    const float up = L.glass.y, gap = L.glass.y - L.tipRoom.h;  // the room's height and its gap to the glass
    for (panel::Box* b : {&L.glass, &L.row, &L.header, &L.toggle})
        if (b->w > 0) b->y -= up;
    for (panel::Box& b : L.cells) b.y -= up;
    for (panel::Box& b : L.rowLabels) b.y -= up;
    L.tipRoom.y = L.glass.y + L.glass.h + gap;
    L.below = true;
}

panel::Box ammoBox(const Layout& L, const Show& show) { return ammoPlace(L, show.ammo); }

std::vector<panel::Box> supplyBoxes(const Layout& L, const Show& show) { return supplyPlaces(L, show); }

Hit hitTest(const Layout& L, float x, float y) {
    if (L.toggle.contains(x, y)) return Hit{Part::Toggle, -1};
    for (size_t i = 0; i < L.cells.size(); ++i)
        if (L.cells[i].contains(x, y)) return Hit{Part::Cell, int(i)};
    return L.glass.contains(x, y) ? Hit{Part::Body, -1} : Hit{};
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Cell>& cells, const Show& show, const Texts& t) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 10 * s, kCard, uistyle::panel(), kWhite, 0.09f, 1.0f * s);
    if (L.header.w > 0) {  // the battle mode's chip
        const float chipW = panel::wordsWidth(t.battle, 12 * s, true) + 18 * s;
        panel::roundRect(out, panel::Box{L.header.x, L.header.y, chipW, L.header.h}, L.header.h / 2, kRed, 0.9f);
    }
    if (L.row.w > 0) panel::roundRect(out, L.row, 7 * s, kWhite, 0.04f);
    for (size_t i = 0; i < L.cells.size(); ++i) {
        const panel::Box b = shown(L, i, show);
        const Cell* c = i < cells.size() ? &cells[i] : nullptr;
        const bool hot = int(i) == show.hover, down = int(i) == show.pressed, chosen = c && c->selected;
        // the concept's states: plain 5 % with a 10 % edge, hovered lighter with 20 %, pressed darker, chosen gold
        const bool lit = show.dropping;  // 3.0.11-beta.110: a drop would take it
        const bool under = int(i) == show.over && show.over != show.lifted;  // 3.0.11-beta.147: a lifted icon over it
        const uint32_t edge = chosen || lit || under ? kGold : kWhite;
        const float edgeA = chosen || under ? 1.0f : lit ? 0.85f : hot ? 0.20f : 0.10f;
        const float edgeW = (chosen || lit || under ? 2.0f : 1.0f) * s;
        if (down) panel::roundRect(out, b, 7 * s, kBlack, 0.30f, edge, edgeA, edgeW);
        else panel::roundRect(out, b, 7 * s, kWhite, hot ? 0.10f : 0.05f, edge, edgeA, edgeW);
        const bool barred = c && (c->notHere != NotHere::No || c->unlearned),
                   dry = c && !barred && (noSp(*c) || !c->lack.empty());  // .13: what it lacks dims it too
        if (c && c->icon && c->icon->width > 0)  // 3.0.11-beta.147: lifted - faded where it was
            put(out, *c->icon, b,
                (barred ? 0.45f : dry ? 0.5f : c->count == 0 ? 0.3f : 1.0f) * (int(i) == show.lifted ? 0.3f : 1.0f),
                barred || dry);
        if (c && c->count < 0 && !c->name.empty() && show.delay > 0) {  // .276: the delay after a skill, clockwise
            const float cx = b.x + b.w / 2, cy = b.y + b.h / 2, rr = 7 * s;
            for (int y = std::max(0, int(b.y)); y < std::min(out.height, int(std::ceil(b.y + b.h))); ++y)
                for (int x = std::max(0, int(b.x)); x < std::min(out.width, int(std::ceil(b.x + b.w))); ++x) {
                    const float px = float(x) + 0.5f, py = float(y) + 0.5f;
                    const float ex = std::max(0.0f, std::max(b.x + rr - px, px - (b.x + b.w - rr)));
                    const float ey = std::max(0.0f, std::max(b.y + rr - py, py - (b.y + b.h - rr)));
                    if (ex * ex + ey * ey > rr * rr) continue;  // outside the rounded corner
                    float turn = std::atan2(px - cx, cy - py) / 6.2831853f;
                    if (turn < 0) turn += 1;
                    if (turn >= 1.0f - show.delay)
                        panel::over(out.px[size_t(y) * size_t(out.width) + size_t(x)], kBlack, 0.55f);
                }
        }
        if (barred) {  // a crossed circle over its icon
            const float side = std::min(b.w, b.h) * 0.7f;
            uiicons::draw(out, uiicons::Icon::Blocked, b.x + (b.w - side) / 2, b.y + (b.h - side) / 2, side, kText,
                          0.9f, 2.0f);
        }
        if (c && !c->key.empty() && !show.pad)  // its key's plate
            panel::roundRect(out, panel::Box{b.x + 2 * s, b.y + 2 * s, panel::wordsWidth(c->key, 11 * s, true) + 6 * s,
                                             13 * s},
                             3 * s, kBlack, 0.62f);
        if (c && c->level > 0)  // a skill's level's plate (the game's words have no shadow)
            panel::roundRect(out, levelBox(b, c->level, s), 3 * s, kBlack, 0.62f);
        if (c && c->count >= 0) {  // an item's count's plate
            const float w = panel::wordsWidth(std::to_string(c->count), 11 * s, true) + 6 * s;
            panel::roundRect(out, panel::Box{b.x + b.w - w - 2 * s, b.y + b.h - 15 * s, w, 13 * s}, 3 * s, kBlack,
                             0.62f);
        }
        if (c && c->count < 0 && !c->standing.empty())  // 3.0.11-beta.143: a ground skill's own count's plate
            panel::roundRect(out, standingBox(b, c->standing, s), 3 * s, kBlack, 0.62f);
        if (c && c->flash > 0)  // 3.0.11-beta.143: a wall of it burnt out - its edge amber, fading
            panel::roundRect(out, b, 7 * s, kAmmoWarn, 0.0f, kAmmoWarn, std::min(1.0f, c->flash), 2.0f * s);
        if (dry && noSp(*c)) panel::roundRect(out, spBox(b, s), 3 * s, kSpBlue, 0.95f);  // its «SP» plate
        else if (dry) panel::roundRect(out, lackBox(b, c->lack, s), 3 * s, kLackRed, 0.95f);  // .13: what it lacks
        if (c && !barred && c->combo > 0 && !chosen) {  // 3.0.16-beta.13: it chains now - gold, a bar shrinking
            panel::roundRect(out, b, 7 * s, kGold2, 0.12f, kGold, 1.0f, 2.0f * s);
            const float bw = (b.w - 8 * s) * std::min(1.0f, c->combo);
            panel::roundRect(out, panel::Box{b.x + 4 * s, b.y + b.h - 6 * s, bw, 3 * s}, 1.5f * s, kGold2, 1.0f);
        } else if (c && !barred && c->ready && !chosen) {  // Asura can go: a blue glow
            panel::roundRect(out, b, 7 * s, kReadyBlue, 0.14f, kReadyBlue, 0.95f, 2.0f * s);
        }
        if (c && c->timer > 0 && !c->timerText.empty()) {  // 3.0.16-beta.22: its wall's or trap's time left
            const float bw = (b.w - 8 * s) * std::min(1.0f, c->timer);
            panel::roundRect(out, panel::Box{b.x + 4 * s, b.y + 3 * s, bw, 3 * s}, 1.5f * s, kGold, 1.0f);
            panel::roundRect(out, timerPlate(b, c->timerText, s), 4 * s, kBlack, 0.72f);
        }
        if (show.pad && c && c->pad < 0) panel::roundRect(out, b, 7 * s, kBlack, 0.5f);  // no button uses it
    }
    for (size_t i = 0; show.pad && i < L.cells.size() && i < cells.size(); ++i)  // 3.0.11-beta.86: the badges
        if (cells[i].pad >= 0) {
            const panel::Box b = shown(L, i, show);
            const padglyph::Family f = show.sony ? padglyph::Family::Sony : padglyph::Family::Xbox;
            padglyph::paint(out, padglyph::Button(cells[i].pad), f, b.x - 4 * s, b.y - 5 * s, 18 * s);
        }
    const float icon = 16 * s;  // 3.0.11-beta.147: the battle mode's button - red as the mode's chip while it is on
    const bool battle = L.mode == Mode::Battle;
    panel::roundRect(out, L.toggle, 7 * s, battle ? kRed : kWhite,
                     battle ? (show.hoverToggle ? 1.0f : 0.9f) : (show.hoverToggle ? 0.14f : 0.06f), kGold,
                     show.hoverToggle ? 0.8f : 0.0f, 1.0f * s);
    uiicons::draw(out, uiicons::Icon::Battle, L.toggle.x + (L.toggle.w - icon) / 2,
                  L.toggle.y + (L.toggle.h - icon) / 2, icon, battle ? kWhite : show.hoverToggle ? kGold2 : kMuted,
                  1.0f, 2.0f);
    const std::vector<TipLine> lines = show.tip && show.hover >= 0 && show.hover < int(cells.size())
                                           ? tipLines(cells[size_t(show.hover)], t)
                                           : std::vector<TipLine>{};
    const panel::Box tip = tipBox(L, show.hover, lines);
    const panel::Box ammo = ammoShown(L, show, tip, t);
    if (ammo.w > 0) {  // 3.0.11-beta.125: the ammo's pill - its edge amber or red when low
        const uint32_t rgb = ammoRgb(show.ammo.count);
        const bool low = show.ammo.count < kAmmoLow;
        panel::roundRect(out, ammo, ammo.h / 2, kCard, std::max(0.85f, uistyle::panel()), low ? rgb : kWhite,
                         low ? 0.9f : 0.12f, (low ? 1.5f : 1.0f) * s);
        if (show.ammo.icon && show.ammo.icon->width > 0)
            put(out, *show.ammo.icon, panel::Box{ammo.x + 8 * s, ammo.y, kAmmoIcon * s, ammo.h}, 1.0f);
    }
    const std::vector<panel::Box> pills = supplyPlaces(L, show);  // 3.0.16-beta.8
    for (size_t i = 0; i < pills.size() && i < show.supplies.size(); ++i) {
        const panel::Box& b = pills[i];
        const Supply& p = show.supplies[i];
        if (overlap(b, tip) || (show.hoverToggle && overlap(b, toggleTip(L, t)))) continue;
        const bool low = p.dots == 0 && p.count <= kAmmoVeryLow;
        const uint32_t rgb = supplyRgb(p);
        panel::roundRect(out, b, b.h / 2, kCard, std::max(0.85f, uistyle::panel()), low ? rgb : kWhite,
                         low ? 0.9f : 0.12f, (low ? 1.5f : 1.0f) * s);
        if (p.dots > 0) {  // the spheres: lit gold, the places left faint
            for (int d = 0; d < p.dots; ++d) {
                const panel::Box dot{b.x + 10 * s + float(d) * (kDot + kDotGap) * s, b.y + (b.h - kDot * s) / 2,
                                     kDot * s, kDot * s};
                if (d < p.count) panel::roundRect(out, dot, dot.h / 2, kGold2, 1.0f);
                else panel::roundRect(out, dot, dot.h / 2, kWhite, 0.08f, kWhite, 0.3f, 1.0f * s);
            }
        } else if (p.icon && p.icon->width > 0) {
            put(out, *p.icon, panel::Box{b.x + 8 * s, b.y, kAmmoIcon * s, b.h}, 1.0f);
        }
    }
    if (tip.w > 0) panel::roundRect(out, tip, 8 * s, kCard, 0.94f, kWhite, 0.12f, 1.0f * s);
    if (show.hoverToggle) panel::roundRect(out, toggleTip(L, t), 8 * s, kCard, 0.94f, kWhite, 0.12f, 1.0f * s);
}

std::vector<Words> words(const Layout& L, const std::vector<Cell>& cells, int row, const Show& show, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    if (L.row.w > 0) {
        out.push_back(Words{panel::Box{L.row.x, L.row.y + 6 * s, L.row.w, 18 * s}, std::to_string(row + 1), kText,
                            15 * s, true, Align::Center});
        out.push_back(Words{panel::Box{L.row.x, L.row.y + L.row.h - 20 * s, L.row.w, 14 * s}, "F12", kMuted, 10.5f * s,
                            false, Align::Center});
    }
    if (L.header.w > 0) {
        const float chipW = panel::wordsWidth(t.battle, 12 * s, true) + 18 * s;
        out.push_back(Words{panel::Box{L.header.x, L.header.y, chipW, L.header.h}, t.battle, kText, 12 * s, true,
                            Align::Center});
        out.push_back(Words{panel::Box{L.header.x + chipW + 8 * s, L.header.y, L.header.w - chipW - 8 * s, L.header.h},
                            t.battleHint, kMuted, 11.5f * s, false, Align::Left});
    }
    for (size_t r = 0; r < L.rowLabels.size(); ++r)
        out.push_back(Words{L.rowLabels[r], std::to_string(gameRow(int(r)) + 1), kText, 13 * s, true, Align::Center});
    for (size_t i = 0; i < L.cells.size() && i < cells.size(); ++i) {
        const panel::Box b = shown(L, i, show);
        const Cell& c = cells[i];
        if (!c.key.empty() && !show.pad)
            out.push_back(Words{panel::Box{b.x + 2 * s, b.y + 2 * s, panel::wordsWidth(c.key, 11 * s, true) + 6 * s,
                                           13 * s},
                                c.key, kGold2, 11 * s, true, Align::Center});
        if (show.pad && c.pad >= 0) {  // 3.0.11-beta.86: its button's letter
            const padglyph::Letter l =
                padglyph::letter(padglyph::Button(c.pad), show.sony ? padglyph::Family::Sony : padglyph::Family::Xbox,
                                 b.x - 4 * s, b.y - 5 * s, 18 * s);
            if (!l.text.empty()) out.push_back(Words{l.box, l.text, l.rgb, l.px, true, Align::Center});
        }
        if (c.count >= 0) {
            const std::string n = std::to_string(c.count);
            const float w = panel::wordsWidth(n, 11 * s, true) + 6 * s;
            out.push_back(Words{panel::Box{b.x + b.w - w - 2 * s, b.y + b.h - 15 * s, w, 13 * s}, n,
                                c.count == 0 ? kMuted : c.low ? kAmmoOut : kText, 11 * s, true, Align::Center});
        }
        if (c.count < 0 && !c.standing.empty())  // 3.0.11-beta.143
            out.push_back(Words{standingBox(b, c.standing, s), c.standing, c.standingOut ? kAmmoOut : kText, 11 * s,
                                true, Align::Center});
        if (c.level > 0)
            out.push_back(Words{levelBox(b, c.level, s), std::to_string(c.level), kText, 11 * s, true, Align::Center});
        if (c.notHere == NotHere::No && !c.unlearned && noSp(c))
            out.push_back(Words{spBox(b, s), "SP", kWhite, 11 * s, true, Align::Center});
        else if (c.notHere == NotHere::No && !c.unlearned && !c.lack.empty())  // 3.0.16-beta.13
            out.push_back(Words{lackBox(b, c.lack, s), c.lack, kWhite, 11 * s, true, Align::Center});
        if (c.timer > 0 && !c.timerText.empty())  // 3.0.16-beta.22
            out.push_back(Words{timerPlate(b, c.timerText, s), c.timerText, kGold, 12 * s, true, Align::Center});
    }
    const std::vector<TipLine> lines = show.tip && show.hover >= 0 && show.hover < int(cells.size())
                                           ? tipLines(cells[size_t(show.hover)], t)
                                           : std::vector<TipLine>{};
    const panel::Box tip = tipBox(L, show.hover, lines);
    if (const panel::Box ammo = ammoShown(L, show, tip, t); ammo.w > 0)  // 3.0.11-beta.125
        out.push_back(Words{panel::Box{ammo.x + (8 + kAmmoIcon + 6) * s, ammo.y, ammo.w - (8 + kAmmoIcon + 6 + 4) * s,
                                       ammo.h},
                            ammoText(show.ammo), ammoRgb(show.ammo.count), kAmmoPx * s, true, Align::Left});
    const std::vector<panel::Box> pills = supplyPlaces(L, show);  // 3.0.16-beta.8: the items' counts
    for (size_t i = 0; i < pills.size() && i < show.supplies.size(); ++i) {
        const panel::Box& b = pills[i];
        const Supply& p = show.supplies[i];
        if (p.dots > 0 || overlap(b, tip) || (show.hoverToggle && overlap(b, toggleTip(L, t)))) continue;
        out.push_back(Words{panel::Box{b.x + (8 + kAmmoIcon + 6) * s, b.y, b.w - (8 + kAmmoIcon + 6 + 4) * s, b.h},
                            supplyText(p), supplyRgb(p), kAmmoPx * s, true, Align::Left});
    }
    for (size_t i = 0; tip.w > 0 && i < lines.size(); ++i)
        out.push_back(Words{panel::Box{tip.x + 10 * s, tip.y + (4 + 18 * float(i)) * s, tip.w - 20 * s, 17 * s},
                            lines[i].text, lines[i].rgb, (i ? 11.5f : 13) * s, i == 0, Align::Left});
    if (show.hoverToggle)
        out.push_back(Words{toggleTip(L, t), toggleText(L, t), kText, 12 * s, false, Align::Center});
    if (show.profile > 1)  // 3.0.11-beta.288: the bar's profile, its number in the toggle's top right corner
        out.push_back(Words{panel::Box{L.toggle.x + L.toggle.w - 6 * s, L.toggle.y - 7 * s, 12 * s, 12 * s},
                            std::to_string(show.profile), kGold2, 10 * s, true, Align::Center});
    return out;
}

std::map<std::string, std::string> nameTable(const std::string& text) {
    std::map<std::string, std::string> out;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = text.substr(at, end - at);
        at = end + 1;
        if (line.rfind("//", 0) == 0) continue;
        const size_t a = line.find('#'), b = a == std::string::npos ? a : line.find('#', a + 1);
        if (a == 0 || b == std::string::npos) continue;
        out[line.substr(0, a)] = line.substr(a + 1, b - a - 1);
    }
    return out;
}

std::string spaced(std::string words) {
    std::replace(words.begin(), words.end(), '_', ' ');
    return words;
}

std::string lowered(std::string text) {
    for (char& c : text)
        if (c >= 'A' && c <= 'Z') c = char(c - 'A' + 'a');
    return text;
}

Slot slotOf(const std::string& raw) {
    Slot s;
    if (raw.size() > 2 && raw[0] == '#' && raw[1] == '#') {
        s.kind = Slot::Kind::Skill;
        s.id = raw.substr(2);
    } else if (!raw.empty() && raw != "nothing" && raw[0] != '#') {
        s.kind = Slot::Kind::Item;
        s.id = raw;
    }
    return s;
}

NotHere notHere(const Slot& slot, const std::vector<std::string>& mapFlags) {
    const auto has = [&](const char* flag) {
        return std::find(mapFlags.begin(), mapFlags.end(), flag) != mapFlags.end();
    };
    const bool war = has("gvg");  // map_flag_gvg without a castle's war (unknown here)
    if (slot.kind == Slot::Kind::Item) {  // pc_isUseitem
        const std::string& n = slot.id;
        if (n == "605") return war ? NotHere::GuildWar : NotHere::No;  // Anodyne
        if (n == "601") return has("noteleport") ? NotHere::Teleport : war ? NotHere::GuildWar : NotHere::No;
        if (n == "602") return has("noreturn") ? NotHere::Return : NotHere::No;  // Butterfly Wing
        if (n == "604" || n == "12024" || n == "12103" || n == "12109")  // the branches, Red Pouch, Poring Box
            return has("nobranch") ? NotHere::Branch : war ? NotHere::GuildWar : NotHere::No;
        return NotHere::No;
    }
    if (slot.kind != Slot::Kind::Skill) return NotHere::No;
    const std::string& k = slot.id;  // skill_not_ok: db/skill_nocast_db.txt's 4 (a war of the guilds) first
    for (const char* inWar : {"AL_TELEPORT", "AL_WARP", "WZ_ICEWALL", "TF_BACKSLIDING", "HP_ASSUMPTIO", "HP_BASILICA",
                              "CG_MOONLIT", "CR_CULTIVATION"})
        if (k == inWar && war) return NotHere::GuildWar;
    if (k == "CG_HERMODE" && !war && !has("gvg_dungeon") && !has("gvg_castle"))  // its 1 and 2: the wars' maps only
        return NotHere::OnlyGuildWar;
    if (k == "AL_WARP") return has("nowarp") ? NotHere::Warp : NotHere::No;
    if (k == "AL_TELEPORT") return has("noteleport") ? NotHere::Teleport : NotHere::No;
    if (k == "MC_VENDING" || k == "MC_IDENTIFY") return NotHere::No;
    if ((k == "WE_CALLPARTNER" || k == "WE_CALLPARENT" || k == "WE_CALLBABY") && has("nomemo")) return NotHere::Call;
    if (k == "WZ_ICEWALL" && has("noicewall")) return NotHere::IceWall;
    return has("noskill") ? NotHere::Skills : NotHere::No;
}

int gameSlot(Mode mode, int row, int cell) {
    if (mode == Mode::Battle) return cell >= 0 && cell < 27 ? gameRow(cell / 9) * 9 + cell % 9 : -1;
    return cell >= 0 && cell < 9 && row >= 0 && row < 3 ? row * 9 + cell : -1;
}

void press(Lift& l, int cell, float x, float y, uint64_t nowMs) {
    l = Lift{};
    l.cell = cell;
    l.x = x;
    l.y = y;
    l.at = nowMs;
}

bool moved(Lift& l, float x, float y, uint64_t nowMs, float scale) {
    if (l.cell < 0) return false;
    if (!l.dragging && nowMs - l.at >= Lift::kLiftMs &&
        std::hypot(x - l.x, y - l.y) >= Lift::kLiftPx * (scale > 0 ? scale : 1.0f))
        l.dragging = true;
    return l.dragging;
}

bool away(const Layout& L, float x, float y) {
    const float m = kAwayPx * L.s;
    const panel::Box& g = L.glass;
    return x < g.x - m || y < g.y - m || x > g.x + g.w + m || y > g.y + g.h + m;
}

Release letGo(const Layout& L, int lifted, float x, float y) {
    if (lifted < 0 || lifted >= int(L.cells.size())) return Release{};
    const Hit h = hitTest(L, x, y);
    if (h.part == Part::Cell) return h.cell == lifted ? Release{} : Release{Let::Swap, h.cell};
    return away(L, x, y) ? Release{Let::Empty, lifted} : Release{};
}

int liftedSide(Mode mode, float scale) { return int(std::lround(cellSide(mode) * (scale > 0 ? scale : 1.0f))); }

void paintLifted(panel::Picture& out, const panel::Picture* icon, bool far, float scale) {
    const float s = scale > 0 ? scale : 1.0f;
    const panel::Box b{0, 0, float(out.width), float(out.height)};
    panel::roundRect(out, b, 7 * s, kCard, 0.75f, kGold, 0.9f, 2.0f * s);
    if (icon && icon->width > 0) put(out, *icon, b, 0.85f);
    if (!far) return;
    const float r = 8 * s;  // let go here - off the bar: a red badge with a minus
    const panel::Box badge{b.w - 2 * r - 1 * s, 1 * s, 2 * r, 2 * r};
    panel::roundRect(out, badge, r, kRed, 1.0f, kWhite, 0.9f, 1.0f * s);
    uiicons::draw(out, uiicons::Icon::Minus, badge.x + 2 * s, badge.y + 2 * s, badge.w - 4 * s, kWhite, 1.0f, 3.0f);
}

}  // namespace uagfx::hotbar
