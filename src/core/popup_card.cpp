#include "core/popup_card.hpp"

#include <algorithm>
#include <cmath>

#include "core/ui_style.hpp"

namespace uagfx::popupcard {
namespace {

// At scale 1: an item's row 28 high, a separator 9; 8 in; the words 14 from the left
constexpr float kRowH = 28, kSepH = 9, kPad = 6, kInset = 14;
constexpr uint32_t kCard = 0x10141C, kWhite = 0xFFFFFF, kGold = 0xC9A55B, kGold2 = 0xE6CB8A, kText = 0xEEF2F8;

}  // namespace

std::vector<Item> items(const std::vector<std::string>& raw, const std::string& separator) {
    std::vector<Item> out;
    for (size_t i = 0; i < raw.size(); ++i) {
        if (raw[i] == separator) {
            out.push_back(Item{"", true});
            continue;
        }
        out.push_back(Item{raw[i], false});
        if (i + 1 < raw.size() && raw[i + 1] == raw[i]) ++i;  // its second half row
    }
    return out;
}

std::string translated(const std::string& text, settings::Lang lang) {
    if (lang == settings::Lang::En) return text;
    using settings::Text;
    static const struct {
        const char* game;
        Text ours;
    } kLines[] = {{"Feed Pet", Text::PetFeed},
                  {"Performance", Text::PetPerform},
                  {"Return to Egg Shell", Text::PetEgg},
                  {"Unequip Accessory", Text::PetUnequip},
                  {"Check the Pet Status", Text::PetStatusCheck}};
    for (const auto& l : kLines)
        if (text == l.game) return settings::tr(lang, l.ours);
    return text;
}

int pickNumber(const std::vector<Item>& items, int row) {
    if (row < 0 || row >= int(items.size()) || items[size_t(row)].separator) return -1;
    int n = 0;
    for (int i = 0; i < row; ++i)
        if (!items[size_t(i)].separator) ++n;
    return n;
}

Layout layout(const std::vector<Item>& items, int atX, int atY, int targetW, int targetH, float scale, float widest) {
    Layout L;
    const float s = scale > 0 ? scale : 1.0f;
    L.s = s;
    const float w = std::max(140 * s, widest + 2 * kInset * s + 2 * kPad * s);
    float y = kPad * s;
    for (const Item& it : items) {
        const float h = (it.separator ? kSepH : kRowH) * s;
        L.rows.push_back(panel::Box{kPad * s, y, w - 2 * kPad * s, h});
        y += h;
    }
    y += kPad * s;
    L.glass = panel::Box{0, 0, w, y};
    L.w = int(std::ceil(w));
    L.h = int(std::ceil(y));
    L.x = std::clamp(atX, 0, std::max(0, targetW - L.w));
    L.y = std::clamp(atY, 0, std::max(0, targetH - L.h));
    return L;
}

int rowAt(const Layout& L, const std::vector<Item>& items, float x, float y) {
    for (size_t i = 0; i < L.rows.size() && i < items.size(); ++i)
        if (L.rows[i].contains(x, y)) return items[i].separator ? -1 : int(i);
    return -1;
}

}  // namespace uagfx::popupcard
