#include "core/item_actions.hpp"

#include <algorithm>

namespace uagfx::itemactions {

itemstate::Where leaving(Kind k) {
    switch (k) {
        case Kind::ToStorage:
        case Kind::ToCart: return itemstate::Where::Bag;
        case Kind::FromCart:
        case Kind::CartToStorage: return itemstate::Where::Cart;
        default: return itemstate::Where::Storage;
    }
}

Result Queue::ask(Kind k, uint16_t index, uint32_t amount, uint32_t have, bool connected, uint32_t nowMs,
                  itempackets::Bytes& out) {
    out.clear();
    const itemstate::Where where = leaving(k);
    if (k != Kind::CloseStorage) {
        if (have == 0) return Result::NothingToMove;
        if (waiting(where, index)) return Result::Busy;
    }
    if (!connected) return Result::NotConnected;
    const uint32_t n = std::min(std::max<uint32_t>(amount, 1), have);
    switch (k) {
        case Kind::ToStorage: out = itempackets::toStorage(index, n); break;
        case Kind::FromStorage: out = itempackets::fromStorage(index, n); break;
        case Kind::CloseStorage: out = itempackets::closeStorage(); break;
        case Kind::ToCart: out = itempackets::toCart(index, n); break;
        case Kind::FromCart: out = itempackets::fromCart(index, n); break;
        case Kind::StorageToCart: out = itempackets::storageToCart(index, n); break;
        case Kind::CartToStorage: out = itempackets::cartToStorage(index, n); break;
    }
    if (k != Kind::CloseStorage) waits_.push_back(Wait{k, where, index, nowMs});
    return Result::Sent;
}

Result Queue::askDeal(bool buy, const itempackets::Pairs& rows, bool connected, uint32_t nowMs,
                      itempackets::Bytes& out) {
    out.clear();
    if (rows.empty()) return Result::NothingToMove;
    if (dealAt_) return Result::Busy;
    if (!connected) return Result::NotConnected;
    out = buy ? itempackets::buyList(rows) : itempackets::sellList(rows);
    dealAt_ = nowMs + 1;
    return Result::Sent;
}

Outcome Queue::onChange(const itemstate::Change& c) {
    if (c.where == itemstate::Where::Shop) {  // 3.0.24-beta.9: the deal's answer
        if (!dealAt_) return Outcome::None;
        dealAt_ = 0;
        lastResult_ = c.result;
        return c.result == 0 ? Outcome::Done : Outcome::Refused;
    }
    if (c.closed) {  // the storage gone (or the cart emptied): what waited on it ends
        const auto gone = std::remove_if(waits_.begin(), waits_.end(),
                                         [&c](const Wait& w) { return w.where == c.where; });
        if (gone == waits_.end()) return Outcome::None;
        last_ = *gone;
        waits_.erase(gone, waits_.end());
        return Outcome::Dropped;
    }
    if (c.delta >= 0) return Outcome::None;  // a thing coming in ends nothing: the leaving side's answer does
    const auto at = std::find_if(waits_.begin(), waits_.end(),
                                 [&](const Wait& w) { return w.where == c.where && w.index == c.index; });
    if (at == waits_.end()) return Outcome::None;
    last_ = *at;
    waits_.erase(at);
    return Outcome::Done;
}

Outcome Queue::tick(uint32_t nowMs) {
    if (dealAt_ && nowMs + 1 - dealAt_ >= kTimeoutMs) {  // 3.0.24-beta.9: the deal unanswered
        dealAt_ = 0;
        lastResult_ = -1;
        return Outcome::TimedOut;
    }
    const auto old = std::find_if(waits_.begin(), waits_.end(),
                                  [nowMs](const Wait& w) { return nowMs - w.at >= kTimeoutMs; });
    if (old == waits_.end()) return Outcome::None;
    last_ = *old;
    last_.ms = nowMs - old->at;
    waits_.erase(old);
    return Outcome::TimedOut;
}

bool Queue::waiting(itemstate::Where w, uint16_t index) const {
    return std::any_of(waits_.begin(), waits_.end(),
                       [&](const Wait& x) { return x.where == w && x.index == index; });
}

}  // namespace uagfx::itemactions
