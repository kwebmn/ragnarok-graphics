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
// Its answer once: -2 none yet, -1 no answer in time, 0 done, 1 zeny, 2 weight, 3 other (a shop's: 4 its stock)
int takeDealAnswer();

}  // namespace uagfx::win::itemnet
