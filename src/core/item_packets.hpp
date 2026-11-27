#pragma once
#include <cstdint>
#include <utility>
#include <vector>

namespace uagfx::itempackets {

// 3.0.24-beta.4 (docs/superpowers/specs/2026-11-25-items-via-packets-design.md, stage 1 «Склад»): the packets the
// driver sends for the things itself, in kRO Sakexe 2007-05-07's numbering (PACKETVER 7: uAthena's packet_db.txt rows
// for that client - movetokafra 0x0094 14 bytes, the index at 7, the amount at 10; movefromkafra 0x00f7 22, at 14
// and 18; closekafra 0x0193 2), and the server's packets' lengths (−1 their own, at +2; 0 not one of these)
using Bytes = std::vector<uint8_t>;
Bytes toStorage(uint16_t bagIndex, uint32_t amount);
Bytes fromStorage(uint16_t storageIndex, uint32_t amount);
Bytes closeStorage();
// 3.0.24-beta.8 (stage 2 «Візок»): 8 bytes each, the index at 2, the amount at 4 - putitemtocart 0x0126,
// getitemfromcart 0x0127, movefromkafratocart 0x0128, movetokafrafromcart 0x0129
Bytes toCart(uint16_t bagIndex, uint32_t amount);
Bytes fromCart(uint16_t cartIndex, uint32_t amount);
Bytes storageToCart(uint16_t storageIndex, uint32_t amount);
Bytes cartToStorage(uint16_t cartIndex, uint32_t amount);
// 3.0.24-beta.9 (stage 3 «Магазин»): the NPC deal in one packet - npcbuylistsend 0x00c8 {amount.W, item.W}...,
// npcselllistsend 0x00c9 {index.W, amount.W}... (the length at 2); pairs given as {item, amount} / {index, amount}
using Pairs = std::vector<std::pair<uint16_t, uint16_t>>;
Bytes buyList(const Pairs& itemAmount);
Bytes sellList(const Pairs& indexAmount);
// 3.0.24-beta.18 (stage 4 «Лавка»): purchasereq 0x0134 - the length, the merchant's account, {amount.W, place.W}
// (pairs given as {place, amount}: the place as the shop's list 0x0133 gave it, the cart's index + 2)
Bytes vendBuy(uint32_t merchant, const Pairs& placeAmount);
// 3.0.24-beta.20 (docs/superpowers/plans/2026-11-27-item-windows-old-layout.md, task 2): «Речі»'s own, in our
// client's section (packet_db.txt packet_ver 22) - useitem 0x009f 14 {index@4, account@10} (as the pad's
// uapad::buildUseItem), dropitem 0x0116 10 {index@5, amount@8}, equipitem 0x00a9 6 {index@2, places@4}, unequipitem
// 0x00ab 4 {index@2}; the index the bag's (place + 2, as the game's drags and lists give it)
Bytes useItem(uint16_t index, uint32_t account);
Bytes drop(uint16_t index, uint16_t amount);
Bytes equip(uint16_t index, uint16_t places);
Bytes unequip(uint16_t index);
int serverLength(uint16_t id);

}  // namespace uagfx::itempackets
