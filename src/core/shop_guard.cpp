#include "core/shop_guard.hpp"

#include "core/item_window.hpp"

namespace uagfx::shopguard {

Ask askBeforeSelling(int refine, int cards) {
    if (cards > 0) return Ask::Card;
    if (refine >= 5) return Ask::Refine;
    return Ask::None;
}

bool sellable(bool favourite) { return !favourite; }

bool misc(int type) { return itemwin::tabOf(type) == itemwin::Tab::Etc; }

int cardsIn(const int cards[4]) {
    if (cards[0] == 255 || cards[0] == 254 || cards[0] == -256) return 0;
    int n = 0;
    for (int i = 0; i < 4; ++i) n += cards[i] >= 4001 && cards[i] <= 4999 ? 1 : 0;
    return n;
}

bool Confirm::pass(int ware, Ask a, uint32_t nowMs) {
    if (a == Ask::None) return true;
    if (ware == ware_ && nowMs - at_ < kConfirmMs) {
        ware_ = -1;
        return true;
    }
    ware_ = ware;
    at_ = nowMs;
    return false;
}

}  // namespace uagfx::shopguard
