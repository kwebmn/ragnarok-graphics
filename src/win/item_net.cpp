#include "win/item_net.hpp"

#include <windows.h>

#include <algorithm>

#include "core/item_packets.hpp"
#include "core/settings/strings.hpp"
#include "pad/win/runtime.hpp"
#include "win/log.hpp"
#include "win/words.hpp"

namespace uagfx::win::itemnet {
namespace {

// The game's packet loop's buffer (the loop's [ebp-0x54b4], win/packet_tap): a packet of its own length is read no
// further than this
constexpr size_t kMostPacket = 0x54B4;
constexpr ULONGLONG kWordsMs = 5000;

struct State {
    itemstate::State items;
    itemactions::Queue queue;
    std::string words;
    ULONGLONG wordsUntil = 0;
    int told = 0;
    int dealAnswer = -2;  // 3.0.24-beta.9
};
State& st() {
    static State s;
    return s;
}

const char* kindName(itemactions::Kind k) {
    switch (k) {
        case itemactions::Kind::ToStorage: return "into the storage";
        case itemactions::Kind::FromStorage: return "out of the storage";
        case itemactions::Kind::CloseStorage: return "the storage closed";
        case itemactions::Kind::ToCart: return "into the cart";
        case itemactions::Kind::FromCart: return "out of the cart";
        case itemactions::Kind::StorageToCart: return "from the storage into the cart";
        case itemactions::Kind::CartToStorage: return "from the cart into the storage";
    }
    return "?";
}

void say(State& s, settings::Text t) {
    s.words = uagfx::win::say(t);
    s.wordsUntil = GetTickCount64() + kWordsMs;
}

}  // namespace

void packet(uint16_t id, const uint8_t* p) {
    State& s = st();
    if (!p) return;
    if (id == 0x0091 || id == 0x0092 || id == 0x0073) {  // another map (or the map server's welcome)
        s.queue.dropAll();
        if (id == 0x0073) s.items.reset();
        else s.items.clearCart();  // 3.0.24-beta.8: its list comes again with the map
        return;
    }
    const int len = itempackets::serverLength(id);
    if (len == 0) return;
    size_t n = len > 0 ? size_t(len) : size_t(p[2] | (p[3] << 8));
    if (n < 2 || n > kMostPacket) return;
    const std::optional<itemstate::Change> c = s.items.feed(p, n);
    if (!c) return;
    if (c->where == itemstate::Where::Shop) {  // 3.0.24-beta.9: the deal answered
        const itemactions::Outcome o = s.queue.onChange(*c);
        if (o == itemactions::Outcome::None) return;  // not ours (the game's own window sent it)
        s.dealAnswer = c->result;
        if (c->result == 1) say(s, settings::Text::ItemsNoZeny);
        else if (c->result == 2) say(s, settings::Text::ItemsTooHeavy);
        else if (c->result != 0) say(s, settings::Text::ItemsShopRefused);
        if (s.told++ < 200) LOG_INFO("items: the NPC deal answered %d (%s)", c->result, c->result ? "refused" : "done");
        return;
    }
    if (s.queue.onChange(*c) != itemactions::Outcome::None && s.told++ < 200)
        LOG_INFO("items: %s, index %u - answered", kindName(s.queue.lastKind()), unsigned(s.queue.lastIndex()));
}

const itemstate::State& state() { return st().items; }

itemactions::Result ask(itemactions::Kind k, uint16_t index, uint32_t amount) {
    State& s = st();
    uint32_t have = 1;
    if (k != itemactions::Kind::CloseStorage) {
        const itemstate::Item* it = s.items.find(itemactions::leaving(k), index);
        have = it ? it->amount : 0;
    }
    itempackets::Bytes out;
    const bool connected = uapad::runtime::healthy();
    itemactions::Result r = s.queue.ask(k, index, amount, have, connected, GetTickCount(), out);
    if (r == itemactions::Result::Sent && !uapad::runtime::sendPacket(out)) {
        s.queue.dropAll();
        r = itemactions::Result::NotConnected;
    }
    if (r == itemactions::Result::NotConnected) say(s, settings::Text::ItemsNotConnected);
    if (s.told++ < 200)
        LOG_INFO("items: %s, index %u, %u of %u - %s", kindName(k), unsigned(index), unsigned(amount), unsigned(have),
                 r == itemactions::Result::Sent           ? "sent"
                 : r == itemactions::Result::Busy         ? "waits for the one before"
                 : r == itemactions::Result::NotConnected ? "no map server connection: not sent"
                                                          : "nothing there");
    return r;
}

bool waiting(itemstate::Where w, uint16_t index) { return st().queue.waiting(w, index); }

itemactions::Result deal(bool buy, const itempackets::Pairs& rows) {
    State& s = st();
    itempackets::Bytes out;
    itemactions::Result r = s.queue.askDeal(buy, rows, uapad::runtime::healthy(), GetTickCount(), out);
    if (r == itemactions::Result::Sent && !uapad::runtime::sendPacket(out)) r = itemactions::Result::NotConnected;
    if (r == itemactions::Result::NotConnected) {
        s.queue.dropAll();
        say(s, settings::Text::ItemsNotConnected);
    }
    if (r == itemactions::Result::Sent) s.dealAnswer = -2;
    if (s.told++ < 200)
        LOG_INFO("items: the NPC deal - %s %zu row(s): %s", buy ? "buy" : "sell", rows.size(),
                 r == itemactions::Result::Sent ? "sent" : r == itemactions::Result::Busy ? "the one before waits"
                                                                                           : "not sent");
    return r;
}

int takeDealAnswer() {
    State& s = st();
    const int a = s.dealAnswer;
    if (a != -2) s.dealAnswer = -2;
    return a;
}

void frame() {
    State& s = st();
    const bool dealWaited = s.queue.dealWaiting();
    if (s.queue.tick(GetTickCount()) != itemactions::Outcome::TimedOut) return;
    if (dealWaited && !s.queue.dealWaiting()) {  // 3.0.24-beta.9: the deal unanswered
        s.dealAnswer = -1;
        say(s, settings::Text::ItemsNoAnswer);
        if (s.told++ < 200) LOG_INFO("items: the NPC deal - no answer in 2 s");
    } else {
        const itemactions::Kind k = s.queue.lastKind();
        say(s, k == itemactions::Kind::ToStorage || k == itemactions::Kind::CartToStorage
                   ? settings::Text::ItemsStorageRefused
               : k == itemactions::Kind::ToCart || k == itemactions::Kind::StorageToCart
                   ? settings::Text::ItemsCartRefused
                   : settings::Text::ItemsNoAnswer);
        if (s.told++ < 200)
            LOG_INFO("items: %s, index %u - no answer in 2 s", kindName(s.queue.lastKind()),
                     unsigned(s.queue.lastIndex()));
    }
}

std::string lastWords() {
    State& s = st();
    return GetTickCount64() < s.wordsUntil ? s.words : std::string();
}

}  // namespace uagfx::win::itemnet
