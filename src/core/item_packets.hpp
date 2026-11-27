#pragma once
#include <cstdint>
#include <utility>
#include <string>
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
// 3.0.24-beta.23 (the spec's «Відкрити лавку»: our own packet): openvending 0x01b2 - its length, the sign (80, a
// nought kept at its end), the flag 1, {the cart's place + 2, amount, price} each 8 bytes. The shop's closing stays
// the game's: the server tells the merchant nothing of it (clif_closevendingboard to the others), so a packet of
// ours would leave the game thinking its shop open
struct VendRow {
    uint16_t place = 0;  // the cart's index + 2, as the cart's list gives it
    uint16_t amount = 0;
    uint32_t price = 0;
};
Bytes openVending(const std::string& sign, const std::vector<VendRow>& rows);
// 3.0.24-beta.24 ([interface] trade_packets): the trade's buttons - «Готово» 0x00eb, «Скасувати» 0x00ed, «Обмін»
// 0x00ef; the server answers both sides (0x00ec, 0x00ee, 0x00f0). The things and the zeny offered stay the game's
// own: it keeps what it offered and applies it at the server's 0x00ea (uaRO.exe 0x586cf6), so ours would leave its
// trade's lists and the zeny shown wrong
// 3.0.24-beta.27 ([interface] party_invite): the party's invite of an account (0x00fc, 6 bytes); the server answers
// the game (0x00fd) and asks the invited (0x00fe)
Bytes partyInvite(uint32_t account);
// 3.0.24-beta.32 (the server's 0x02c4 handler of 27.11): a player asked into the party by name - 0x02c4 <name>.24B
// (the game's code page), its spaces trimmed; none ({}) for no name or one past 23 bytes
Bytes partyInviteByName(const std::string& gameName);
Bytes tradeOk();
Bytes tradeCancel();
Bytes tradeCommit();
Bytes drop(uint16_t index, uint16_t amount);
Bytes equip(uint16_t index, uint16_t places);
Bytes unequip(uint16_t index);
int serverLength(uint16_t id);

}  // namespace uagfx::itempackets
