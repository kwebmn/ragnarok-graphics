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
    ToCart, FromCart, StorageToCart, CartToStorage  // 3.0.24-beta.8: the cart's
};
itemstate::Where leaving(Kind k);  // the side whose answer (its thing going out) ends the action
enum class Result : uint8_t { Sent, Busy, NotConnected, NothingToMove };
enum class Outcome : uint8_t { None, Done, TimedOut, Dropped };
constexpr uint32_t kTimeoutMs = 2000;

class Queue {
public:
    // `have`: how many the cell holds (the amount cut to it; 0 - nothing to move); `out` the packet when Sent
    Result ask(Kind k, uint16_t index, uint32_t amount, uint32_t have, bool connected, uint32_t nowMs,
               itempackets::Bytes& out);
    Outcome onChange(const itemstate::Change& c);
    Outcome tick(uint32_t nowMs);
    bool waiting(itemstate::Where w, uint16_t index) const;
    void dropAll() { waits_.clear(); }
    Kind lastKind() const { return last_.kind; }  // the action an Outcome was about
    uint16_t lastIndex() const { return last_.index; }
    uint32_t lastMs() const { return last_.ms; }  // how long it waited

private:
    struct Wait {
        Kind kind = Kind::ToStorage;
        itemstate::Where where = itemstate::Where::Bag;
        uint16_t index = 0;
        uint32_t at = 0, ms = 0;
    };
    std::vector<Wait> waits_;
    Wait last_;
};

}  // namespace uagfx::itemactions
