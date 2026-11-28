#pragma once
#include <cstdint>

namespace uagfx::shopguard {

// 3.0.24-beta.21 (docs/superpowers/specs/2026-11-27-item-windows-redesign-design.md, «Решено владельцем 27.11»):
// what «Крамниця NPC» guards when selling - «обране» (the items' starred) never in the sale's list; a thing with a
// card always asked, a refine of +5 and over asked - asked as the items' «asked twice»: the second click on the same
// thing within kConfirmMs lets it go (the window's note says why the first did nothing)
enum class Ask : uint8_t { None, Card, Refine };
Ask askBeforeSelling(int refine, int cards);
// 3.0.24-beta.46 (the audit 28.11, the owner's «скрывать»): a worn thing (the worn arrows too) not in the list either
bool sellable(bool favourite, bool worn = false);
bool misc(int type);  // «Різне»: the items' own tab (core/item_window tabOf)
// A thing's real cards in its four slots (4001-4999): a forged one's (255 first), a named one's (254) and an egg's
// (-256) slots hold no card
int cardsIn(const int cards[4]);

constexpr uint32_t kConfirmMs = 3000;
class Confirm {
public:
    bool pass(int ware, Ask a, uint32_t nowMs);  // true: put it in; false: asked (the note says it)

private:
    int ware_ = -1;
    uint32_t at_ = 0;
};

}  // namespace uagfx::shopguard
