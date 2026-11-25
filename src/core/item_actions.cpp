#include "core/item_actions.hpp"

#include <algorithm>

namespace uagfx::itemactions {

Result Queue::ask(Kind k, uint16_t index, uint32_t amount, uint32_t have, bool connected, uint32_t nowMs,
                  itempackets::Bytes& out) {
    out.clear();
    const itemstate::Where where = k == Kind::ToStorage ? itemstate::Where::Bag : itemstate::Where::Storage;
    if (k != Kind::CloseStorage) {
        if (have == 0) return Result::NothingToMove;
        if (waiting(where, index)) return Result::Busy;
    }
    if (!connected) return Result::NotConnected;
    const uint32_t n = std::min(std::max<uint32_t>(amount, 1), have);
    out = k == Kind::ToStorage     ? itempackets::toStorage(index, n)
          : k == Kind::FromStorage ? itempackets::fromStorage(index, n)
                                   : itempackets::closeStorage();
    if (k != Kind::CloseStorage) waits_.push_back(Wait{k, where, index, nowMs});
    return Result::Sent;
}

Outcome Queue::onChange(const itemstate::Change& c) {
    if (c.closed) {  // the storage gone: what waited on it ends
        const auto gone = std::remove_if(waits_.begin(), waits_.end(),
                                         [](const Wait& w) { return w.where == itemstate::Where::Storage; });
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
