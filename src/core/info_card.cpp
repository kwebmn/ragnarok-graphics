#include "core/info_card.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>

#include "core/hotbar_panel.hpp"
#include "core/ui_icons.hpp"
#include "core/ui_style.hpp"

namespace uagfx::infocard {
constexpr uint32_t kGoldLink = 0xE6CB8A;  // 3.0.18-beta.242: the theme's gold (what is pressed)

namespace {

// At scale 1: the card 330 wide, 14 in; its title 48 high (the icon 32); the words 12.5 px a row of 17, at most 16 rows
// shown; the big picture's room 75 x 100 (the game's collection pictures); an item's slots 24 high, 6 apart
constexpr float kW = 330, kSide = 14, kTitleH = 48, kIcon = 32, kTextPx = 12.5f, kLineH = 17, kClose = 24;
constexpr int kMostRows = 20, kWheelRows = 3;  // 3.0.18-beta.108: 20 (the cards' words under the thing's)
constexpr float kPictureW = 75, kPictureH = 100, kSlotH = 34, kSlotGap = 6;  // .109: 34 (a card's picture)
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kText = 0xEEF2F8, kMuted = 0x9AA3B5;
constexpr uint32_t kSpacer = 0xFFFFFF;  // the game's white words: on its white window unseen (spacers)

std::string trimmed(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n')) s.pop_back();
    return s;
}

// The rows of the words wrapped at `width`, word by word, each colour's run apart (npc_talk's way), the white spacers
// left out; a run's x from 0, its y the row's number
std::vector<std::vector<npctalk::Placed>> wrapped(const std::vector<std::string>& lines, float width, float px,
                                                  const npctalk::Measure& m) {
    std::vector<std::vector<npctalk::Placed>> rows;
    for (const std::string& line : lines) {
        rows.emplace_back();
        float x = 0;
        for (const npctalk::Run& r : npctalk::colours(line, kText)) {
            if (r.rgb == kSpacer) continue;
            size_t from = 0;
            while (from < r.text.size()) {
                size_t end = r.text.find(' ', from);
                end = end == std::string::npos ? r.text.size() : end + 1;
                const std::string word = r.text.substr(from, end - from);
                const float ww = m(word, px, false);
                if (x > 0 && x + ww > width) {
                    rows.emplace_back();
                    x = 0;
                }
                std::vector<npctalk::Placed>& row = rows.back();
                if (!row.empty() && row.back().rgb == r.rgb && row.back().box.x + row.back().box.w >= x - 0.5f) {
                    row.back().text += word;
                    row.back().box.w = x + ww - row.back().box.x;
                } else {
                    row.push_back(npctalk::Placed{panel::Box{x, float(rows.size() - 1), ww, 0}, word, r.rgb});
                }
                x += ww;
                from = end;
            }
        }
    }
    return rows;
}

int rowOf(const Layout& L, const npctalk::Placed& p) {
    return L.lineH > 0 ? int(std::lround((p.box.y - L.body.y) / L.lineH)) : 0;
}

}  // namespace

LearnedSkill skillOfCard(const std::string& shown, const std::vector<LearnedSkill>& session,
                         const std::map<std::string, std::string>& names) {
    const auto shownAs = [&](const std::string& aegis) {
        const auto n = names.find(aegis);
        return n != names.end() && hotbar::spaced(n->second) == shown;
    };
    for (const LearnedSkill& k : session)
        if (k.aegis == shown || shownAs(k.aegis)) return k;
    for (const auto& [aegis, name] : names)
        if (aegis == shown || hotbar::spaced(name) == shown) return LearnedSkill{aegis, 0};
    return LearnedSkill{shown, 0};
}

std::map<std::string, std::vector<std::string>> descriptions(const std::string& text) {
    std::map<std::string, std::vector<std::string>> out;
    std::string key;
    std::vector<std::string> lines;
    bool inside = false;
    size_t at = 0;
    while (at <= text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = trimmed(text.substr(at, end - at));
        at = end + 1;
        if (!inside) {
            if (line.rfind("//", 0) == 0 || line.size() < 2 || line.back() != '#') continue;
            key = line.substr(0, line.size() - 1);
            if (key.find('#') != std::string::npos) continue;
            lines.clear();
            inside = true;
        } else if (line == "#") {
            out[key] = lines;
            inside = false;
        } else {
            lines.push_back(line);
        }
        if (end == text.size()) break;
    }
    return out;
}

std::vector<std::string> forJob(const std::vector<std::string>& lines, int job,
                                const std::function<std::string(int)>& name) {
    const auto jobOf = [](const std::string& l, size_t& digits) {  // the job's number before a colour, -1 none
        digits = 0;
        while (digits < l.size() && digits < 5 && l[digits] >= '0' && l[digits] <= '9') ++digits;
        return digits > 0 && digits < l.size() && l[digits] == '^' ? std::atoi(l.substr(0, digits).c_str()) : -1;
    };
    bool mine = false;
    size_t n = 0;
    for (const std::string& l : lines) mine = mine || (job >= 0 && jobOf(l, n) == job);
    std::vector<std::string> out;
    for (const std::string& l : lines) {
        const int j = jobOf(l, n);
        if (j < 0) {
            out.push_back(l);
        } else if (mine) {
            if (j == job) out.push_back(l.substr(n));
        } else {
            const std::string who = name ? name(j) : std::string();
            out.push_back((who.empty() ? "#" + std::to_string(j) : who) + ": " + l.substr(n));
        }
    }
    return out;
}

std::map<int, int> slotCounts(const std::string& text) {
    std::map<int, int> out;
    size_t at = 0;
    while (at < text.size()) {
        size_t end = text.find('\n', at);
        if (end == std::string::npos) end = text.size();
        const std::string line = trimmed(text.substr(at, end - at));
        at = end + 1;
        const size_t a = line.find('#'), b = a == std::string::npos ? a : line.find('#', a + 1);
        if (a == 0 || b == std::string::npos || line.rfind("//", 0) == 0) continue;
        char* stop = nullptr;
        const long id = std::strtol(line.c_str(), &stop, 10);
        if (stop != line.c_str() + a || id <= 0) continue;
        out[int(id)] = std::atoi(line.c_str() + a + 1);
    }
    return out;
}

std::string languageFolder(unsigned langId) {
    switch (langId & 0x3FF) {
        case 0x07: return "german\\";
        case 0x10: return "italian\\";
        case 0x16: return "portuguese\\";
        case 0x1F: return "turkish\\";
        default: return "english\\";
    }
}

int iconSide(float scale) { return int(std::lround(kIcon * (scale > 0 ? scale : 1.0f))); }

void pictureRoom(float scale, int& w, int& h) {
    const float s = scale > 0 ? scale : 1.0f;
    w = int(std::lround(kPictureW * s));
    h = int(std::lround(kPictureH * s));
}

void artRoom(float scale, int& w, int& h) {
    const float s = scale > 0 ? scale : 1.0f;
    w = int(std::lround(180 * s));
    h = int(std::lround(240 * s));
}

panel::Picture fitted(const std::vector<uint32_t>& argb, int width, int height, int w, int h) {
    panel::Picture out;
    if (width <= 0 || height <= 0 || w <= 0 || h <= 0 || argb.size() != size_t(width) * size_t(height)) return out;
    const float k = std::min(float(w) / float(width), float(h) / float(height));
    panel::blank(out, std::max(1, int(std::lround(float(width) * k))), std::max(1, int(std::lround(float(height) * k))));
    for (int y = 0; y < out.height; ++y)
        for (int x = 0; x < out.width; ++x) {  // bilinear from its own pixels, premultiplied as it goes
            const float sx = std::clamp((float(x) + 0.5f) / k - 0.5f, 0.0f, float(width - 1));
            const float sy = std::clamp((float(y) + 0.5f) / k - 0.5f, 0.0f, float(height - 1));
            const int x0 = int(sx), y0 = int(sy), x1 = std::min(width - 1, x0 + 1), y1 = std::min(height - 1, y0 + 1);
            const float fx = sx - float(x0), fy = sy - float(y0);
            float acc[4] = {0, 0, 0, 0};
            const int xs[2] = {x0, x1}, ys[2] = {y0, y1};
            const float wx[2] = {1 - fx, fx}, wy[2] = {1 - fy, fy};
            for (int j = 0; j < 2; ++j)
                for (int i = 0; i < 2; ++i) {
                    const uint32_t c = argb[size_t(ys[j]) * size_t(width) + size_t(xs[i])];
                    const float wgt = wx[i] * wy[j], a = float(c >> 24) / 255.0f;
                    acc[0] += wgt * float(c >> 24);
                    acc[1] += wgt * float((c >> 16) & 0xFF) * a;
                    acc[2] += wgt * float((c >> 8) & 0xFF) * a;
                    acc[3] += wgt * float(c & 0xFF) * a;
                }
            const auto b = [](float v) { return uint32_t(std::clamp(v + 0.5f, 0.0f, 255.0f)); };
            out.px[size_t(y) * size_t(out.width) + size_t(x)] = b(acc[0]) << 24 | b(acc[1]) << 16 | b(acc[2]) << 8 |
                                                                  b(acc[3]);
        }
    return out;
}

Layout layout(const Card& c, int x, int y, int targetW, int targetH, float scale, const Show& sh, const Texts& t,
              const npctalk::Measure& m) {
    (void)sh;
    (void)t;
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = kW * s, side = kSide * s, titleH = kTitleH * s, icon = kIcon * s;
    L.icon = panel::Box{side, (titleH - icon) / 2, icon, icon};
    L.close = panel::Box{w - side - kClose * s + 4 * s, (titleH - kClose * s) / 2, kClose * s, kClose * s};
    const float tx = side + icon + 10 * s, tw = L.close.x - 6 * s - tx;
    L.title = panel::Box{tx, c.note.empty() ? (titleH - 18 * s) / 2 : 6 * s, tw, 18 * s};
    if (!c.note.empty()) L.note = panel::Box{tx, 25 * s, tw, 16 * s};
    const float bodyY = titleH + 6 * s;
    float pw = 0, ph = 0;
    float wordsY = bodyY;
    if (c.picture && c.picture->width > 0 && c.art) {  // 3.0.18-beta.110: a card's illustration over its words
        int rw = 0, rh = 0;
        artRoom(s, rw, rh);
        L.picture = panel::Box{(w - float(rw)) / 2, bodyY, float(rw), float(rh)};
        wordsY = bodyY + float(rh) + 10 * s;
    } else if (c.picture && c.picture->width > 0) {
        int rw = 0, rh = 0;
        pictureRoom(s, rw, rh);
        pw = float(rw);
        ph = float(rh);
        L.picture = panel::Box{side, bodyY, pw, ph};  // 3.0.18-beta.109: on the left, as the game's own window
    }
    const float textX = side + (pw > 0 ? pw + 10 * s : 0.0f);
    const float textW = w - side - textX;
    L.lineH = kLineH * s;
    const auto rows = wrapped(c.lines, textW, kTextPx * s, m);
    L.rows = int(rows.size());
    L.shownRows = std::min(L.rows, kMostRows);
    for (size_t r = 0; r < rows.size(); ++r)
        for (npctalk::Placed p : rows[r]) {
            p.box.x += textX;
            p.box.y = wordsY + float(r) * L.lineH;
            p.box.h = L.lineH;
            L.lines.push_back(p);
        }
    const float bodyH = std::max(float(L.shownRows) * L.lineH, ph);
    L.body = panel::Box{textX, wordsY, textW, bodyH};
    float bottom = wordsY + bodyH;
    if (!c.cards.empty()) {  // the slots: a label, then the cards in a row (four to a row)
        bottom += 10 * s;
        L.slotsLabel = panel::Box{side, bottom, w - 2 * side, 16 * s};
        bottom += 18 * s;
        const int across = 2;  // 3.0.18-beta.109: two a row - a card's picture and its name
        const float cw = (w - 2 * side - float(across - 1) * kSlotGap * s) / float(across);
        for (size_t i = 0; i < c.cards.size(); ++i) {
            const float sx = side + float(i % across) * (cw + kSlotGap * s);
            const float sy = bottom + float(i / across) * (kSlotH + kSlotGap) * s;
            L.slots.push_back(panel::Box{sx, sy, cw, kSlotH * s});
        }
        bottom += float((c.cards.size() + across - 1) / across) * (kSlotH + kSlotGap) * s - kSlotGap * s;
    }
    if (!t.reference.empty()) {  // 3.0.18-beta.242: the link to its card in «Довідник», at the bottom left
        bottom += 10 * s;
        L.reference = panel::Box{side, bottom, std::min(w - 2 * side, std::ceil(m(t.reference, 12 * s, true)) + 4 * s),
                                 18 * s};
        bottom += 18 * s;
    }
    L.glass = panel::Box{0, 0, w, bottom + side};
    L.w = int(std::ceil(L.glass.w));
    L.h = int(std::ceil(L.glass.h));
    L.x = std::clamp(x, 0, std::max(0, targetW - L.w));
    L.y = std::clamp(y, 0, std::max(0, targetH - L.h));
    return L;
}

Part hitTest(const Layout& L, float x, float y) {
    if (L.close.contains(x, y)) return Part::Close;
    if (L.reference.w > 0 && L.reference.contains(x, y)) return Part::Reference;  // 3.0.18-beta.242
    // 3.0.18-beta.110: the head's band (a card's illustration under it is the body's)
    const float head = std::max(L.title.y + L.title.h, L.note.y + L.note.h) + 6 * L.s;
    if (y < head && L.glass.contains(x, y)) return Part::Title;
    return L.glass.contains(x, y) ? Part::Body : Part::None;
}

int slotAt(const Layout& L, float x, float y) {
    for (size_t i = 0; i < L.slots.size(); ++i)
        if (L.slots[i].contains(x, y)) return int(i);
    return -1;
}

int scrolled(const Layout& L, int scroll, int notches) {
    return std::clamp(scroll + notches * kWheelRows, 0, std::max(0, L.rows - L.shownRows));
}

std::vector<Words> words(const Layout& L, const Card& c, const Show& sh, const Texts& t) {
    std::vector<Words> out;
    const float s = L.s;
    out.push_back(Words{L.title, c.title, kText, 14 * s, true, Align::Left});
    if (!c.note.empty()) out.push_back(Words{L.note, c.note, kMuted, 12 * s, false, Align::Left});
    for (const npctalk::Placed& p : L.lines) {
        const int row = rowOf(L, p);
        if (row < sh.scroll || row >= sh.scroll + L.shownRows) continue;
        panel::Box b = p.box;
        b.y -= float(sh.scroll) * L.lineH;
        b.w += 2 * s;  // GDI's rounding: a run's last letter not cut
        out.push_back(Words{b, p.text, p.rgb, kTextPx * s, false, Align::Left});
    }
    if (!L.slots.empty()) out.push_back(Words{L.slotsLabel, t.slots, kMuted, 11.5f * s, true, Align::Left});
    for (size_t i = 0; i < L.slots.size() && i < c.cards.size(); ++i) {
        const bool filled = !c.cards[i].empty();
        // 3.0.18-beta.109: a card's picture at the slot's left (win/info_card), its name beside it
        const bool pic = filled && i < c.cardIcons.size() && c.cardIcons[i];
        const panel::Box b = pic ? panel::Box{L.slots[i].x + 32 * s, L.slots[i].y, L.slots[i].w - 34 * s, L.slots[i].h}
                                 : L.slots[i];
        out.push_back(Words{b, filled ? c.cards[i] : t.empty, filled ? kText : kMuted, 10.5f * s, filled,
                            pic ? Align::Left : Align::Center});
    }
    if (L.reference.w > 0)  // 3.0.18-beta.242: gold - it is pressed
        out.push_back(Words{L.reference, t.reference, kGoldLink, 12 * s, true, Align::Left});
    return out;
}

}  // namespace uagfx::infocard
