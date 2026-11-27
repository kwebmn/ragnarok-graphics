#include "core/item_ui.hpp"

#include <algorithm>

namespace uagfx::itemui {
namespace {

struct Width {
    uint32_t letter;
    int regular, semibold;
};
constexpr Width kPlex[] = {
#include "core/plex_widths.inc"
};

bool inFont(uint32_t c) {
    if (c == '\n' || c == '\r' || c == '\t') return true;
    const auto it = std::lower_bound(std::begin(kPlex), std::end(kPlex), c,
                                     [](const Width& w, uint32_t x) { return w.letter < x; });
    return it != std::end(kPlex) && it->letter == c;
}

}  // namespace

settings::Text hintOf(Window w) {
    using settings::Text;
    switch (w) {
        case Window::Items: return Text::ItemHintItems;
        case Window::Equip: return Text::ItemHintEquip;
        case Window::Storage: return Text::ItemHintStorage;
        case Window::Cart: return Text::ItemHintCart;
        case Window::ShopBuy: return Text::ItemHintShopBuy;
        case Window::ShopSell: return Text::ItemHintShopSell;
        case Window::Vend: return Text::ItemHintVend;
        case Window::VendSetup: return Text::ItemHintVendSetup;
        case Window::MyShop: return Text::ItemHintMyShop;
        case Window::Trade: return Text::ItemHintTrade;
        case Window::Amount: return Text::ItemHintAmount;
        case Window::Card: return Text::ItemHintCard;
        case Window::Pick: return Text::ItemHintPick;
        case Window::Count: break;
    }
    return Text::ItemHintItems;
}

Fill fillOf(int64_t used, int64_t most) {
    Fill f;
    if (most <= 0) return f;
    f.percent = int(std::clamp<int64_t>(used * 100 / most, 0, 100));
    f.tone = f.percent >= 90 ? "full" : f.percent >= 70 ? "warn" : "ok";
    return f;
}

std::string countTitle(const std::string& name, int used, int most) {
    return name + " " + std::to_string(used) + "/" + std::to_string(most);
}

bool fontHas(const std::string& utf8) {
    for (size_t i = 0; i < utf8.size();) {
        const auto b = static_cast<unsigned char>(utf8[i]);
        const int n = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 0;
        if (n == 0 || i + size_t(n) > utf8.size()) return false;
        uint32_t c = n == 1 ? b : n == 2 ? b & 0x1F : n == 3 ? b & 0x0F : b & 0x07;
        for (int k = 1; k < n; ++k) {
            const auto x = static_cast<unsigned char>(utf8[i + size_t(k)]);
            if ((x >> 6) != 2) return false;
            c = c << 6 | (x & 0x3F);
        }
        if (!inFont(c)) return false;
        i += size_t(n);
    }
    return true;
}

}  // namespace uagfx::itemui
