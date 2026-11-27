#pragma once
#include <cstdint>
#include <string>

#include "core/item_actions.hpp"
#include "core/item_state.hpp"

namespace uagfx::win::itemnet {

// 3.0.24-beta.4 (docs/superpowers/specs/2026-11-25-items-via-packets-design.md, stage 1 «Склад»): the driver's own
// hold on the things - the server's packets (win/packet_tap) make the bag and the storage (core/item_state); an action
// goes to the map server by the pad's session (uapad::runtime::sendPacket) and waits for its answer
// (core/item_actions). On the game's thread.
void packet(uint16_t id, const uint8_t* p);
const itemstate::State& state();
itemactions::Result ask(itemactions::Kind k, uint16_t index, uint32_t amount);
bool waiting(itemstate::Where w, uint16_t index);
void frame();  // the waits' time-outs
// The window's words after an action that did not end well ("" none; kept 5 s)
std::string lastWords();
// 3.0.24-beta.9 (stage 3 «Магазин»): an NPC deal in one packet ({item, amount} bought / {index, amount} sold)
itemactions::Result deal(bool buy, const itempackets::Pairs& rows);
// 3.0.24-beta.18 (stage 4 «Лавка»): a purchase at a player's shop ({place, amount}: the place as its list gave it)
itemactions::Result vend(uint32_t merchant, const itempackets::Pairs& rows);
// 3.0.24-beta.20 (docs/superpowers/plans/2026-11-27-item-windows-old-layout.md, task 2): «Речі»'s and
// «Спорядження»'s own - a bag thing used, worn (at `places`), taken off, dropped; the cell waits for the server
// 3.0.24-beta.22 (the spec's «Скільки штук»: «предел по весу и месту: «у склад влізе 87»»): how many of `count` of a
// thing (`id`) fit where it goes - the bag (its weight, 100 places), the cart (its weight and places as 0x0121 tells),
// the storage (its places); `words` say it when fewer than the stack ("" else)
struct Fit {
    int most = 0;
    std::string words;
};
Fit fitIn(itemstate::Where to, int id, int count);
itemactions::Result use(uint16_t index);
itemactions::Result equip(uint16_t index, uint16_t places);
itemactions::Result unequip(uint16_t index);
itemactions::Result drop(uint16_t index, uint32_t amount);
// 3.0.24-beta.23 ([interface] vend_open_packets): the shop opened by our own packet 0x01b2 (no wait: the server's
// answer 0x0136 opens the game's «Моя лавка»); false: not sent (no connection)
bool openShop(const std::string& sign, const std::vector<itempackets::VendRow>& rows);
// 3.0.24-beta.24: a packet of ours with no wait (the trade's buttons); false: not sent (no connection)
bool sendOwn(const itempackets::Bytes& bytes);
// Its answer once: -2 none yet, -1 no answer in time, 0 done, 1 zeny, 2 weight, 3 other (a shop's: 4 its stock)
int takeDealAnswer();

}  // namespace uagfx::win::itemnet
