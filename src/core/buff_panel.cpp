#include "core/buff_panel.hpp"

#include <algorithm>
#include <iterator>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::buffpanel {
namespace {

// 3.0.11-beta.231 (the owner: «на черной плашке все както не красиво, в оригинале сделано лучше»; the concept
// «Бафы и дебафы», variant C): no glass, no rings - each icon straight on the picture as the game's own, a dark
// outline and shadow round it, the time gone darkening it clockwise from twelve, its numbers over its foot; a
// favourite 36, the rest 30; the names list a round button of its own over the icons
constexpr float kFrameBig = 36, kFrameSmall = 30, kPad = 3, kGap = 5, kTimeH = 14, kSep = 6;
constexpr float kColumnW = kFrameBig + 2 * kPad, kNamesW = 204, kNameGap = 8;
constexpr float kToggleH = 22, kToggleGap = 6;
constexpr float kTipW = 230, kTipGap = 6, kTipH = 42, kTipLine = 26, kRowTip = 50, kRowGap = 6;
constexpr float kMoreH = 28, kMoreW = 40;  // 3.0.11-beta.63: «ще N» 28 high (the concept's «не ниже 28 px»)
constexpr float kLineGap = 6;               // between the columns (the rows): two columns 114 wide, as the concept's
constexpr float kRadius = 10, kFrameRadius = 8, kIconRadius = 5;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000, kTile = 0x1B1D24;
constexpr uint32_t kGold = 0xE6CB8A, kWarn = 0xF0A050, kRed = 0xFF5A4E, kRedText = 0xFF9A8F;
constexpr uint32_t kText = 0xECEFF5, kMuted = 0x9AA3B5;

float frameOf(Kind k) { return k == Kind::Favourite ? kFrameBig : kFrameSmall; }

// A rounded box's signed distance (inside negative) and a pixel's share of the inside
float roundBox(float x, float y, const panel::Box& b, float r) {
    r = std::min(r, std::min(b.w, b.h) / 2);
    const float qx = std::fabs(x - (b.x + b.w / 2)) - (b.w / 2 - r);
    const float qy = std::fabs(y - (b.y + b.h / 2)) - (b.h / 2 - r);
    const float ox = std::max(qx, 0.0f), oy = std::max(qy, 0.0f);
    return std::sqrt(ox * ox + oy * oy) + std::min(std::max(qx, qy), 0.0f) - r;
}
float cover(float d) { return std::clamp(0.5f - d, 0.0f, 1.0f); }

uint32_t premultiplied(uint32_t argb) {
    const uint32_t a = argb >> 24;
    const auto ch = [&](int sh) { return ((argb >> sh) & 0xFF) * a / 255; };
    return a << 24 | ch(16) << 16 | ch(8) << 8 | ch(0);
}

// A column's (the row's) entries: its favourites, the rest, its debuffs; whether «ще N» stands in it
struct Line {
    std::vector<int> fav, rest, deb;
    bool more = false;
};
struct Plan {
    std::vector<Line> lines;  // the first, then the further ones
    int hidden = 0;
};

void add(Line& l, const std::vector<Entry>& e, int i) {
    (e[size_t(i)].kind == Kind::Favourite ? l.fav : e[size_t(i)].kind == Kind::Debuff ? l.deb : l.rest).push_back(i);
}

// Along the column (or the row) at scale 1: a line's glass's length
float lengthOf(const std::vector<Entry>& e, const Line& l, Look look) {
    const bool row = look == Look::Row;
    float len = 2 * kPad;
    int items = 0, groups = 0;
    for (const std::vector<int>* list : {&l.fav, &l.rest, &l.deb}) {
        if (list->empty()) continue;
        ++groups;
        for (const int i : *list) {
            const float f = frameOf(e[size_t(i)].kind);
            len += f;
            ++items;
        }
    }
    if (l.more) {
        len += row ? kMoreW : kMoreH;
        ++items;
    }
    const float gap = row ? kRowGap : kGap;
    if (items > 1) len += gap * float(items - 1);
    if (groups > 1) len += float(groups - 1) * (kSep + gap);
    return len;
}

// `items` in lines of their own after the plan's, each as many as `room` holds (one at the least)
void fill(Plan& p, const std::vector<Entry>& e, const std::vector<int>& items, Look look, float room) {
    for (size_t from = 0; from < items.size();) {
        Line l;
        add(l, e, items[from++]);
        for (; from < items.size(); ++from) {
            Line longer = l;
            add(longer, e, items[from]);
            if (lengthOf(e, longer, look) > room) break;
            l = std::move(longer);
        }
        p.lines.push_back(std::move(l));
    }
}

// The first line: the debuffs at its end, as many of the others over them as `room` holds (one at the least) - the
// rest's last ones out first, then the favourites' - those out under «ще N» or in further lines; the debuffs past
// its room in lines of their own next to it
Plan planOf(const std::vector<Entry>& e, Look look, Overflow overflow, float room) {
    std::vector<int> others, deb;  // the favourites and the rest; the debuffs - each as order() puts them
    for (const int i : order(e)) (e[size_t(i)].kind == Kind::Debuff ? deb : others).push_back(i);
    const bool hide = overflow == Overflow::More;
    Line first;
    size_t kept = others.size();
    for (;; --kept) {
        first = Line{};
        for (size_t k = 0; k < kept; ++k) add(first, e, others[k]);
        first.deb = deb;
        first.more = hide && kept < others.size();
        if (lengthOf(e, first, look) <= room || kept == 0 || (kept == 1 && deb.empty())) break;
    }
    size_t debKept = deb.size();
    while (debKept > 1 && lengthOf(e, first, look) > room) first.deb.resize(--debKept);
    Plan p;
    p.lines.push_back(std::move(first));
    fill(p, e, std::vector<int>(deb.begin() + std::ptrdiff_t(debKept), deb.end()), look, room);
    if (hide)
        p.hidden = int(others.size() - kept);
    else
        fill(p, e, std::vector<int>(others.begin() + std::ptrdiff_t(kept), others.end()), look, room);
    return p;
}

std::string fmtCount(const std::string& text, int n) {
    std::string out = text;
    const size_t at = out.find("{1}");
    if (at != std::string::npos) out.replace(at, 3, std::to_string(n));
    return out;
}

}  // namespace

bool debuff(uint16_t icon) {
    switch (icon) {
        case 0:    // SI_PROVOKE
        case 8:    // SI_QUAGMIRE
        case 11:   // SI_SIGNUMCRUCIS
        case 13:   // SI_DECREASEAGI
        case 22:   // SI_AETERNA
        case 32:   // SI_BROKENARMOR
        case 33:   // SI_BROKENWEAPON
        case 34:   // SI_HALLUCINATION
        case 35:   // SI_WEIGHT50
        case 36:   // SI_WEIGHT90
        case 50:   // SI_STRIPWEAPON
        case 51:   // SI_STRIPSHIELD
        case 52:   // SI_STRIPARMOR
        case 53:   // SI_STRIPHELM
        case 95:   // SI_STOP
        case 124:  // SI_BLEEDING
        case 125:  // SI_JOINTBEAT
        case 199:  // SI_WINKCHARM
        case 201:  // SI_CLOSECONFINE2 (its target)
        case 282:  // SI_SLOWCAST
        case 286:  // SI_CRITICALWOUND
            return true;
        default: return false;
    }
}

const char* stateName(uint16_t icon) {
    switch (icon) {
        case 27: return "Riding";
        case 28: return "Falcon";
        case 32: return "Broken Armor";
        case 33: return "Broken Weapon";
        case 34: return "Hallucination";
        case 35: return "Weight 70 %";  // 3.0.11-beta.202: uAthena on uaRO sends it at 70 %
        case 36: return "Weight 90 %";
        case 95: return "Stop";
        case 124: return "Bleeding";
        default: break;
    }
    // 3.0.11-beta.232 (R-DS774A: Auto Berserk's «#132»): the skill giving it, as the server's status.c has it
    struct IconName {
        uint16_t icon;
        const char* name;
    };
#include "core/buff_icon_names.inc"
    const auto at = std::lower_bound(std::begin(kIconNames), std::end(kIconNames), icon,
                                     [](const IconName& e, uint16_t v) { return e.icon < v; });
    return at != std::end(kIconNames) && at->icon == icon ? at->name : "";
}

std::vector<int> order(const std::vector<Entry>& entries) {
    std::vector<int> out;
    for (const Kind k : {Kind::Favourite, Kind::Buff, Kind::Debuff})
        for (size_t i = 0; i < entries.size(); ++i)
            if (entries[i].kind == k) out.push_back(int(i));
    return out;
}

int iconSide(Kind kind, float scale) {
    return int(std::lround(float(kind == Kind::Favourite ? kBig : kSmall) * scale));
}

namespace {

// A line's slots along its glass: its kinds with a line between them, «ще N» after the non-debuffs (over the
// debuffs when only they stand there)
void place(Layout& L, const std::vector<Entry>& entries, const Line& line, const panel::Box& glass) {
    const float s = L.s, pad = kPad * s;
    const bool row = L.look == Look::Row;
    const float gap = (row ? kRowGap : kGap) * s;
    float at = (row ? glass.x : glass.y) + pad;
    std::vector<const std::vector<int>*> groups;
    for (const std::vector<int>* list : {&line.fav, &line.rest, &line.deb})
        if (!list->empty()) groups.push_back(list);
    const bool onlyDebuffs = line.fav.empty() && line.rest.empty();
    const auto placeMore = [&] {
        L.more = row ? panel::Box{at, glass.y + pad, kMoreW * s, kFrameSmall * s}
                     : panel::Box{glass.x + pad, at, glass.w - 2 * pad, kMoreH * s};
        at += (row ? kMoreW : kMoreH) * s + gap;
    };
    if (line.more && onlyDebuffs) placeMore();
    for (size_t g = 0; g < groups.size(); ++g) {
        if (g) {  // the line between this kind and the last
            const float mid = at + (kSep * s - 1.0f * s) / 2;
            L.seps.push_back(row ? panel::Box{mid, glass.y + pad, 1.0f * s, glass.h - 2 * pad}
                                 : panel::Box{glass.x + pad, mid, glass.w - 2 * pad, 1.0f * s});
            at += kSep * s + gap;
        }
        for (const int i : *groups[g]) {
            Slot slot;
            slot.entry = i;
            const float f = frameOf(entries[size_t(i)].kind) * s;
            // the icons' middles in line: the column's (the names') at the glass's right, the row's on its middle
            const float mid = glass.x + glass.w - pad - kFrameBig * s / 2;
            if (row) {
                slot.frame = panel::Box{at, glass.y + glass.h / 2 - f / 2, f, f};
                at += f + gap;
            } else {
                slot.frame = panel::Box{mid - f / 2, at, f, f};
                if (L.look == Look::Names)
                    slot.name = panel::Box{glass.x, at, slot.frame.x - kNameGap * s - glass.x, f};
                at += f + gap;
            }
            slot.time = panel::Box{slot.frame.x - 6 * s, slot.frame.y + f - kTimeH * s, f + 12 * s, kTimeH * s};
            slot.icon = slot.frame;
            L.slots.push_back(slot);
        }
        // the debuffs, when there, are the last kind: «ще N» after the kind before them, else after the last
        const size_t lastKept = groups.size() - (line.deb.empty() ? 1 : 2);
        if (line.more && !onlyDebuffs && g == lastKept) placeMore();
    }
}

}  // namespace

Layout layout(const std::vector<Entry>& entries, Look look, int right, int top, int bottom, int widest, float scale,
              Overflow overflow) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.look = look;
    const bool row = look == Look::Row;
    // a line's room at scale 1: the row as many across as `widest` holds (0: all), the column down to `bottom`
    const float room =
        row ? (widest > 0 ? float(widest) / s : 1e9f) : float(bottom - top) / s - (kToggleH + kToggleGap);
    const Plan plan = planOf(entries, look, overflow, room);
    L.hidden = plan.hidden;
    const size_t n = plan.lines.size();
    std::vector<panel::Box> glass(n);
    const float lineGap = kLineGap * s;
    if (row) {
        float most = kTipW * s, y = 0;
        for (const Line& l : plan.lines) most = std::max(most, lengthOf(entries, l, look) * s);
        L.w = int(std::ceil(most));
        for (size_t i = 0; i < n; ++i) {  // each under the last, at the right; a favourite's taller
            const float length = lengthOf(entries, plan.lines[i], look) * s;
            const float frame = plan.lines[i].fav.empty() ? kFrameSmall : kFrameBig;
            const float h = 2 * kPad * s + frame * s;
            glass[i] = panel::Box{float(L.w) - length, y, length, h};
            y += h + lineGap;
        }
        const float under = glass[n - 1].y + glass[n - 1].h;
        L.h = int(std::ceil(under + kRowTip * s));
        L.tipRoom = panel::Box{0, under, float(L.w), float(L.h) - under};
    } else {
        const float bodyW = (look == Look::Names ? kNamesW : kColumnW) * s;
        const float toggleW = kToggleH * s;
        const float tip = look == Look::Column ? (kTipW + kTipGap) * s : 0.0f;
        const float span = float(n) * bodyW + float(n - 1) * lineGap;
        L.w = int(std::ceil(tip + std::max(span, toggleW)));
        const float bodyY = (kToggleH + kToggleGap) * s;
        float longest = 0;
        for (size_t i = 0; i < n; ++i) {  // each left of the last, their tops in line
            const float length = lengthOf(entries, plan.lines[i], look) * s;
            glass[i] = panel::Box{float(L.w) - bodyW - float(i) * (bodyW + lineGap), bodyY, bodyW, length};
            longest = std::max(longest, length);
        }
        L.h = int(std::ceil(bodyY + longest));
        // over the icons' middle (the names' and the column's icons stand at the glass's right alike)
        L.toggle = panel::Box{float(L.w) - kPad * s - (kFrameBig * s + toggleW) / 2, 0, toggleW, kToggleH * s};
        if (look == Look::Column) L.tipRoom = panel::Box{0, bodyY, glass[n - 1].x - kTipGap * s, longest};
    }
    L.x = right - L.w;
    L.y = top;
    L.body = glass[0];
    L.lines.assign(glass.begin() + 1, glass.end());
    for (size_t i = 0; i < n; ++i) place(L, entries, plan.lines[i], glass[i]);
    const float x0 = std::min(L.body.x, L.toggle.w > 0 ? L.toggle.x : L.body.x);
    const float y0 = L.toggle.w > 0 ? L.toggle.y : L.body.y;
    L.hit = panel::Box{x0, y0, L.body.x + L.body.w - x0, L.body.y + L.body.h - y0};
    if (!L.lines.empty()) {
        float lx = L.lines[0].x, ly = L.lines[0].y, lx1 = lx, ly1 = ly;
        for (const panel::Box& b : L.lines) {
            lx = std::min(lx, b.x);
            ly = std::min(ly, b.y);
            lx1 = std::max(lx1, b.x + b.w);
            ly1 = std::max(ly1, b.y + b.h);
        }
        L.linesHit = panel::Box{lx, ly, lx1 - lx, ly1 - ly};
    }
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.toggle.w > 0 && L.toggle.contains(x, y)) return Hit{Part::Toggle, -1};
    if (L.more.w > 0 && L.more.contains(x, y)) return Hit{Part::More, -1};
    for (size_t i = 0; i < L.slots.size(); ++i) {
        const Slot& s = L.slots[i];
        panel::Box b = s.frame;  // the icon, its time over its foot, its name left of it (the names)
        const float x1 = std::max({s.frame.x + s.frame.w, s.time.x + s.time.w, s.name.x + s.name.w});
        const float y1 = std::max({s.frame.y + s.frame.h, s.time.y + s.time.h, s.name.y + s.name.h});
        b.x = std::min({b.x, s.time.x, s.name.w > 0 ? s.name.x : b.x});
        b.w = x1 - b.x;
        b.h = y1 - b.y;
        if (b.contains(x, y)) return Hit{Part::Icon, int(i)};
    }
    return Hit{};
}

float wordsWidth(const std::string& utf8, float px, bool bold) { return panel::wordsWidth(utf8, px, bold); }

panel::Box tipBox(const Layout& L, int slot, float width) {
    if (slot < 0 || slot >= int(L.slots.size())) return panel::Box{};
    const Slot& s = L.slots[size_t(slot)];
    const float w = width + 20 * L.s, h = kTipH * L.s;
    if (L.look == Look::Row) {  // under the rows
        const float x = std::clamp(s.frame.x + s.frame.w / 2 - w / 2, 0.0f, std::max(0.0f, float(L.w) - w));
        return panel::Box{x, L.tipRoom.y + 6 * L.s, w, h};
    }
    const float right = (L.lines.empty() ? L.body : L.lines.back()).x - kTipGap * L.s;  // left of the columns
    const float x = std::max(0.0f, right - w);
    return panel::Box{x, s.frame.y + s.frame.h / 2 - h / 2, right - x, h};
}

namespace {

// The tip's two lines and its box: the name and the time; a favourite's click off, the rest's on; a debuff none
panel::Box tipLines(const Layout& L, const std::vector<Entry>& entries, int hover, const Texts& t, std::string& first,
                    std::string& second) {
    const float s = L.s;
    if (hover == kToggleTip && L.toggle.w > 0) {  // 3.0.11-beta.231: what the round button does, left of it
        first = L.look == Look::Names ? t.hideNames : t.names;
        second.clear();
        const float w = wordsWidth(first, 13 * s, true) + 20 * s, h = kTipLine * s;
        return panel::Box{std::max(0.0f, L.toggle.x - kTipGap * s - w), L.toggle.y + (L.toggle.h - h) / 2, w, h};
    }
    if (hover < 0 || hover >= int(L.slots.size()) || L.look == Look::Names) return panel::Box{};
    const Entry& e = entries[size_t(L.slots[size_t(hover)].entry)];
    first = e.time.empty() ? e.name : e.name + " \xC2\xB7 " + e.time;
    second = e.kind == Kind::Debuff ? std::string() : e.kind == Kind::Favourite ? t.unstar : t.star;
    const float width = std::max(wordsWidth(first, 13 * s, true), wordsWidth(second, 11.5f * s, false));
    panel::Box b = tipBox(L, hover, width);
    if (second.empty()) b.h = kTipLine * s, b.y += (kTipH - kTipLine) / 2 * s;
    return b;
}

}  // namespace

std::vector<Words> words(const Layout& L, const std::vector<Entry>& entries, int hover, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    if (L.more.w > 0)  // 3.0.11-beta.231: on the picture, its dark outline round the words
        out.push_back(Words{L.more, fmtCount(t.more, L.hidden), kText, 12 * s, true, Align::Center, true});
    for (const Slot& slot : L.slots) {
        const Entry& e = entries[size_t(slot.entry)];
        const bool bad = e.kind == Kind::Debuff;
        const uint32_t timeRgb = bad ? kRedText : e.warn ? kWarn : kText;
        if (L.look == Look::Names)  // left of its icon, right-aligned, outlined
            out.push_back(Words{slot.name, e.name, bad ? kRedText : kText, 13 * s, true, Align::Right, true});
        if (!e.time.empty())  // over the icon's foot, outlined
            out.push_back(Words{slot.time, e.time, timeRgb, 11.5f * s, true, Align::Center, true});
    }
    std::string first, second;
    const panel::Box b = tipLines(L, entries, hover, t, first, second);
    if (b.w > 0) {
        out.push_back(Words{panel::Box{b.x + 10 * s, b.y + 4 * s, b.w - 20 * s, 18 * s}, first, kText, 13 * s, true,
                            Align::Left});
        if (!second.empty())
            out.push_back(Words{panel::Box{b.x + 10 * s, b.y + 22 * s, b.w - 20 * s, 15 * s}, second, kMuted,
                                11.5f * s, false, Align::Left});
    }
    return out;
}

panel::Box tipText(const Layout& L, const std::vector<Entry>& entries, int hover, const Texts& t, std::string& first,
                   std::string& second) {
    return tipLines(L, entries, hover, t, first, second);
}

panel::Box tipOf(const Layout& L, const std::vector<Entry>& entries, int hover, const Texts& t) {
    std::string first, second;
    return tipLines(L, entries, hover, t, first, second);
}

panel::Picture iconPicture(const std::vector<uint32_t>& argb, int width, int height, bool alpha, int side,
                           float radius) {
    panel::Picture p;
    if (width <= 0 || height <= 0 || side <= 0 || argb.size() < size_t(width) * size_t(height)) return p;
    p.width = p.height = side;
    p.px.assign(size_t(side) * size_t(side), 0u);
    std::vector<uint32_t> src(argb.size());  // premultiplied, the magenta keyed out of a picture without alpha
    for (size_t i = 0; i < argb.size(); ++i) {
        uint32_t c = argb[i];
        if (!alpha) {
            const uint32_t r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
            c = r > 0xF0 && g < 0x10 && b > 0xF0 ? 0u : (c | 0xFF000000u);
        }
        src[i] = premultiplied(c);
    }
    const float sx = float(width) / float(side), sy = float(height) / float(side);
    const panel::Box all{0, 0, float(side), float(side)};
    for (int y = 0; y < side; ++y)
        for (int x = 0; x < side; ++x) {  // each pixel the source's area under it, weighed by its share
            const float x0 = float(x) * sx, x1 = float(x + 1) * sx, y0 = float(y) * sy, y1 = float(y + 1) * sy;
            float acc[4] = {0, 0, 0, 0}, total = 0;
            for (int v = int(y0); v < std::min(height, int(std::ceil(y1))); ++v)
                for (int u = int(x0); u < std::min(width, int(std::ceil(x1))); ++u) {
                    const float wx = std::min(x1, float(u + 1)) - std::max(x0, float(u));
                    const float wy = std::min(y1, float(v + 1)) - std::max(y0, float(v));
                    const float w = wx * wy;
                    if (w <= 0) continue;
                    const uint32_t c = src[size_t(v) * size_t(width) + size_t(u)];
                    for (int k = 0; k < 4; ++k) acc[k] += w * float((c >> (24 - 8 * k)) & 0xFF);
                    total += w;
                }
            if (total <= 0) continue;
            const float k = radius > 0 ? cover(roundBox(float(x) + 0.5f, float(y) + 0.5f, all, radius)) : 1.0f;
            uint32_t c = 0;
            for (int i = 0; i < 4; ++i)
                c |= uint32_t(std::clamp(acc[i] / total * k + 0.5f, 0.0f, 255.0f)) << (24 - 8 * i);
            p.px[size_t(y) * size_t(side) + size_t(x)] = c;
        }
    return p;
}

}  // namespace uagfx::buffpanel
