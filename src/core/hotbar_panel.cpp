#include "core/hotbar_panel.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_icons.hpp"

namespace uagfx::hotbar {
namespace {

// The concept's sizes at scale 1 (v17_hotbar): a cell 48 (its icon 32), the battle mode's 44 (30), 4 apart; the bar 6
// in; the row's chip 34 wide; the edit button 28; a tip 42 high over the bar
constexpr float kCell = 48, kCellBattle = 44, kGap = 4, kPad = 6, kRowW = 34, kLabelW = 18, kEdit = 28;
constexpr float kHeaderH = 22, kTipH = 42;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kBlack = 0x000000;
constexpr uint32_t kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xECEFF5, kMuted = 0x9AA3B5, kRed = 0xC0392B;

const char* const kBattleKeys[3][9] = {{"Q", "W", "E", "R", "T", "Y", "U", "I", "O"},
                                       {"A", "S", "D", "F", "G", "H", "J", "K", "L"},
                                       {"Z", "X", "C", "V", "B", "N", "M", ",", "."}};

float cellSide(Mode m) { return m == Mode::Battle ? kCellBattle : kCell; }

// A premultiplied picture centred in a box, its alpha times `k`
void put(panel::Picture& out, const panel::Picture& pic, const panel::Box& box, float k) {
    const int ox = int(std::lround(box.x + (box.w - float(pic.width)) / 2));
    const int oy = int(std::lround(box.y + (box.h - float(pic.height)) / 2));
    for (int y = 0; y < pic.height; ++y)
        for (int x = 0; x < pic.width; ++x) {
            const int tx = ox + x, ty = oy + y;
            if (tx < 0 || ty < 0 || tx >= out.width || ty >= out.height) continue;
            uint32_t c = pic.px[size_t(y) * size_t(pic.width) + size_t(x)];
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

// The hovered cell's tip: its words, its box over the bar
std::vector<std::string> tipLines(const Cell& c, const Texts& t) {
    if (c.name.empty()) return {};
    std::string first = c.name;
    if (c.level > 0) first += " \xC2\xB7 Lv " + std::to_string(c.level);
    if (c.count >= 0) first += " \xC2\xB7 \xC3\x97" + std::to_string(c.count);
    return {first, t.use};
}

panel::Box tipBox(const Layout& L, int hover, const std::vector<std::string>& lines) {
    if (hover < 0 || hover >= int(L.cells.size()) || lines.empty()) return panel::Box{};
    float w = 0;
    for (size_t i = 0; i < lines.size(); ++i)
        w = std::max(w, panel::wordsWidth(lines[i], (i ? 11.5f : 13) * L.s, i == 0));
    w += 20 * L.s;
    const panel::Box& c = L.cells[size_t(hover)];
    const float x = std::clamp(c.x + c.w / 2 - w / 2, 0.0f, std::max(0.0f, float(L.w) - w));
    return panel::Box{x, L.tipRoom.y + L.tipRoom.h - kTipH * L.s, w, kTipH * L.s};
}

// The edit button's tip over it
panel::Box editTip(const Layout& L, const Texts& t) {
    const std::string& text = L.mode == Mode::Editing ? t.done : t.edit;
    const float w = panel::wordsWidth(text, 12 * L.s, false) + 20 * L.s;
    const float x = std::clamp(L.edit.x + L.edit.w / 2 - w / 2, 0.0f, std::max(0.0f, float(L.w) - w));
    return panel::Box{x, L.tipRoom.y + L.tipRoom.h - 26 * L.s, w, 22 * L.s};
}

}  // namespace

int iconSide(Mode mode, float scale) { return int(std::lround((mode == Mode::Battle ? 30 : 32) * scale)); }

int gameRow(int shownRow) { return shownRow == 0 ? 1 : shownRow == 1 ? 2 : 0; }

const char* battleKey(int shownRow, int cell) {
    return shownRow >= 0 && shownRow < 3 && cell >= 0 && cell < 9 ? kBattleKeys[shownRow][cell] : "";
}

Layout layout(Mode mode, int centreX, int bottom, float scale) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    L.mode = mode;
    const float pad = kPad * s, tip = kTipH * s + 6 * s;
    if (mode == Mode::Editing) {  // the game's own bar shows: the done button alone
        L.glass = panel::Box{0, tip, kEdit * s + 2 * pad, kEdit * s + 2 * pad};
        L.edit = panel::Box{pad, tip + pad, kEdit * s, kEdit * s};
    } else {
        const float side = cellSide(mode) * s, gap = kGap * s;
        const float cellsW = 9 * side + 8 * gap;
        const bool battle = mode == Mode::Battle;
        const float leftW = (battle ? kLabelW : kRowW) * s;
        const float w = pad + leftW + pad + cellsW + pad + kEdit * s + pad;
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
        L.edit = panel::Box{x0 + cellsW + pad, top + ((battle ? 3 * side + 2 * gap : side) - kEdit * s) / 2, kEdit * s,
                            kEdit * s};
    }
    L.tipRoom = panel::Box{0, 0, L.glass.w, tip - 6 * s};
    L.w = int(std::ceil(std::max(L.glass.w, 300 * s)));
    // the glass centred in the picture
    const float shift = (float(L.w) - L.glass.w) / 2;
    for (panel::Box* b : {&L.glass, &L.row, &L.header, &L.edit}) b->x += shift;
    for (panel::Box& b : L.cells) b.x += shift;
    for (panel::Box& b : L.rowLabels) b.x += shift;
    L.tipRoom.w = float(L.w);
    L.h = int(std::ceil(L.glass.y + L.glass.h));
    L.x = centreX - L.w / 2;
    L.y = bottom - L.h;
    return L;
}

Hit hitTest(const Layout& L, float x, float y) {
    if (L.edit.contains(x, y)) return Hit{Part::Edit, -1};
    for (size_t i = 0; i < L.cells.size(); ++i)
        if (L.cells[i].contains(x, y)) return Hit{Part::Cell, int(i)};
    return L.glass.contains(x, y) ? Hit{Part::Body, -1} : Hit{};
}

void paint(panel::Picture& out, const Layout& L, const std::vector<Cell>& cells, int hover, bool hoverEdit,
           const Texts& t) {
    const float s = L.s;
    panel::roundRect(out, L.glass, 10 * s, kCard, 0.86f, kWhite, 0.09f, 1.0f * s);
    if (L.header.w > 0) {  // the battle mode's chip
        const float chipW = panel::wordsWidth(t.battle, 12 * s, true) + 18 * s;
        panel::roundRect(out, panel::Box{L.header.x, L.header.y, chipW, L.header.h}, L.header.h / 2, kRed, 0.9f);
    }
    if (L.row.w > 0) panel::roundRect(out, L.row, 7 * s, kWhite, 0.04f);
    for (size_t i = 0; i < L.cells.size(); ++i) {
        const panel::Box& b = L.cells[i];
        const Cell* c = i < cells.size() ? &cells[i] : nullptr;
        const bool hot = int(i) == hover;
        panel::roundRect(out, b, 7 * s, kWhite, hot ? 0.10f : 0.05f, hot ? kGold : kWhite, hot ? 0.95f : 0.10f,
                         (hot ? 1.5f : 1.0f) * s);
        if (c && c->icon && c->icon->width > 0) put(out, *c->icon, b, c->count == 0 ? 0.3f : 1.0f);
        if (c && !c->key.empty())  // its key's plate
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
    }
    const float icon = 16 * s;
    panel::roundRect(out, L.edit, 7 * s, kWhite, hoverEdit ? 0.14f : 0.06f, kGold, hoverEdit ? 0.8f : 0.0f, 1.0f * s);
    uiicons::draw(out, L.mode == Mode::Editing ? uiicons::Icon::Done : uiicons::Icon::Edit,
                  L.edit.x + (L.edit.w - icon) / 2, L.edit.y + (L.edit.h - icon) / 2, icon,
                  hoverEdit || L.mode == Mode::Editing ? kGold2 : kMuted, 1.0f, 2.0f);
    const std::vector<std::string> lines =
        hover >= 0 && hover < int(cells.size()) ? tipLines(cells[size_t(hover)], t) : std::vector<std::string>{};
    const panel::Box tip = tipBox(L, hover, lines);
    if (tip.w > 0) panel::roundRect(out, tip, 8 * s, kCard, 0.94f, kWhite, 0.12f, 1.0f * s);
    if (hoverEdit) panel::roundRect(out, editTip(L, t), 8 * s, kCard, 0.94f, kWhite, 0.12f, 1.0f * s);
}

std::vector<Words> words(const Layout& L, const std::vector<Cell>& cells, int row, int hover, bool hoverEdit,
                         const Texts& t) {
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
        const panel::Box& b = L.cells[i];
        const Cell& c = cells[i];
        if (!c.key.empty())
            out.push_back(Words{panel::Box{b.x + 2 * s, b.y + 2 * s, panel::wordsWidth(c.key, 11 * s, true) + 6 * s,
                                           13 * s},
                                c.key, kGold2, 11 * s, true, Align::Center});
        if (c.count >= 0) {
            const std::string n = std::to_string(c.count);
            const float w = panel::wordsWidth(n, 11 * s, true) + 6 * s;
            out.push_back(Words{panel::Box{b.x + b.w - w - 2 * s, b.y + b.h - 15 * s, w, 13 * s}, n,
                                c.count == 0 ? kMuted : kText, 11 * s, true, Align::Center});
        }
        if (c.level > 0)
            out.push_back(Words{levelBox(b, c.level, s), std::to_string(c.level), kText, 11 * s, true, Align::Center});
    }
    const std::vector<std::string> lines =
        hover >= 0 && hover < int(cells.size()) ? tipLines(cells[size_t(hover)], t) : std::vector<std::string>{};
    const panel::Box tip = tipBox(L, hover, lines);
    for (size_t i = 0; tip.w > 0 && i < lines.size(); ++i)
        out.push_back(Words{panel::Box{tip.x + 10 * s, tip.y + (4 + 18 * float(i)) * s, tip.w - 20 * s, 17 * s},
                            lines[i], i ? kMuted : kText, (i ? 11.5f : 13) * s, i == 0, Align::Left});
    if (hoverEdit)
        out.push_back(Words{editTip(L, t), L.mode == Mode::Editing ? t.done : t.edit, kText, 12 * s, false,
                            Align::Center});
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

}  // namespace uagfx::hotbar
