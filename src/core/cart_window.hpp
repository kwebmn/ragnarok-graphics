#pragma once
#include <cstdint>
#include <string>

namespace uagfx::cartwindow {

// 3.0.11-beta.154 (task 4 of the remaining windows, the merchant's: the cart): «Візок» - the merchant's cart in
// core/item_window's grid (win/cart_window). The game's facts it works by (FUN_004d42d0 0x28, the cart window's press
// FUN_004b7850, the game mode's 0x59 - FUN_005707d0 - and the session's list FUN_0062ac80)
constexpr int kGameWindow = 0x28;          // UI window 0x28: the cart («UIMerchantItemWnd»)
constexpr uint32_t kManagerOffset = 0x140;  // where the windows' manager keeps it
constexpr int kDragMsg = 0x59;             // the game mode's: a cart thing's drag begun (its place)
constexpr int kDragKind = 9;               // that drag's kind (the bag's drop takes 6 and 9: the storage's, the cart's)
constexpr uint32_t kSessionList = 0xF4C, kSessionCount = 0xF50;  // the session's cart list and its count

// Its title: the name and the things in it of the most («Візок · 12/100»); the name alone when nothing is known yet
std::string title(const std::string& name, int count, int most);

}  // namespace uagfx::cartwindow
