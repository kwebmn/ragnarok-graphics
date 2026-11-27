#pragma once
#include <cstdint>
#include <vector>

#include "core/item_packets.hpp"
#include "core/item_state.hpp"

namespace uagfx::itemactions {

// 3.0.24-beta.4 (the design's «Действия»: «одно нажатие — одно действие», «окно говорит почему»): an action on a
// thing becomes the server's packet and waits for the answer that ends it - the server's own change of that cell
// (core/item_state Change) - or kTimeoutMs; while it waits, the same cell asks nothing more. Nothing is drawn ahead
// of the answer.
enum class Kind : uint8_t {
    ToStorage, FromStorage, CloseStorage,
    ToCart, FromCart, StorageToCart, CartToStorage,  // 3.0.24-beta.8: the cart's
    Use, Equip, Unequip, Drop                        // 3.0.24-beta.20: «Речі»'s (the bag's own)
};
itemstate::Where leaving(Kind k);  // the side whose answer (its thing going out) ends the action
enum class Result : uint8_t { Sent, Busy, NotConnected, NothingToMove };
enum class Outcome : uint8_t { None, Done, TimedOut, Dropped, Refused };  // .9: Refused - a deal's answer not 0
constexpr uint32_t kTimeoutMs = 2000;

class Queue {
public:
    // `have`: how many the cell holds (the amount cut to it; 0 - nothing to move); `out` the packet when Sent
    Result ask(Kind k, uint16_t index, uint32_t amount, uint32_t have, bool connected, uint32_t nowMs,
               itempackets::Bytes& out);
    // 3.0.24-beta.9 (stage 3 «Магазин»): an NPC deal - the cart's rows in one packet (buy: {item, amount}, sell:
    // {index, amount}); one at a time, ended by its answer (Done when 0, Refused with lastResult() else)
    Result askDeal(bool buy, const itempackets::Pairs& rows, bool connected, uint32_t nowMs, itempackets::Bytes& out);
    // 3.0.24-beta.18 (stage 4 «Лавка»): a purchase at a player's shop ({place, amount} rows); it has no answer of its
    // own when it goes through - the things coming into the bag end it (Done), 0x0135 refuses it (Refused)
    Result askVend(uint32_t merchant, const itempackets::Pairs& rows, bool connected, uint32_t nowMs,
                   itempackets::Bytes& out);
    // 3.0.24-beta.20 (docs/superpowers/plans/2026-11-27-item-windows-old-layout.md, task 2): a bag thing used, worn,
    // taken off, dropped - each its packet, ended at its cell: used by what is left (0x01c8) or refused (0x00a8),
    // worn and taken off by 0x00aa / 0x00ac (Done or Refused), dropped by its going out (0x00af)
    Result askUse(uint16_t index, uint32_t account, bool connected, uint32_t nowMs, itempackets::Bytes& out);
    Result askEquip(uint16_t index, uint16_t places, bool connected, uint32_t nowMs, itempackets::Bytes& out);
    Result askUnequip(uint16_t index, bool connected, uint32_t nowMs, itempackets::Bytes& out);
    Result askDrop(uint16_t index, uint32_t amount, uint32_t have, bool connected, uint32_t nowMs,
                   itempackets::Bytes& out);
    int lastResult() const { return lastResult_; }
    bool dealWaiting() const { return dealAt_ != 0; }
    Outcome onChange(const itemstate::Change& c);
    Outcome tick(uint32_t nowMs);
    bool waiting(itemstate::Where w, uint16_t index) const;
    void dropAll() {  // 3.0.24-beta.18: a deal waiting too (another map: no answer will come)
        waits_.clear();
        dealAt_ = 0;
        vend_ = false;
    }
    Kind lastKind() const { return last_.kind; }  // the action an Outcome was about
    uint16_t lastIndex() const { return last_.index; }
    uint32_t lastMs() const { return last_.ms; }  // how long it waited

private:
    Result own(Kind k, uint16_t index, bool connected, uint32_t nowMs);  // the bag's: Busy, NotConnected or kept
    struct Wait {
        Kind kind = Kind::ToStorage;
        itemstate::Where where = itemstate::Where::Bag;
        uint16_t index = 0;
        uint32_t at = 0, ms = 0;
    };
    std::vector<Wait> waits_;
    Wait last_;
    uint32_t dealAt_ = 0;  // the deal sent then (+1: never 0), 0 none waits
    bool vend_ = false;    // 3.0.24-beta.18: the deal waiting is a vending purchase
    int lastResult_ = -1;
};

}  // namespace uagfx::itemactions
