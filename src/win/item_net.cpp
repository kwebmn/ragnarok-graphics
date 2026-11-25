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
};
State& st() {
    static State s;
    return s;
}

const char* kindName(itemactions::Kind k) {
    return k == itemactions::Kind::ToStorage ? "into the storage"
           : k == itemactions::Kind::FromStorage ? "out of the storage"
                                                 : "the storage closed";
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
        return;
    }
    const int len = itempackets::serverLength(id);
    if (len == 0) return;
    size_t n = len > 0 ? size_t(len) : size_t(p[2] | (p[3] << 8));
    if (n < 2 || n > kMostPacket) return;
    const std::optional<itemstate::Change> c = s.items.feed(p, n);
    if (!c) return;
    if (s.queue.onChange(*c) != itemactions::Outcome::None && s.told++ < 200)
        LOG_INFO("items: %s, index %u - answered", kindName(s.queue.lastKind()), unsigned(s.queue.lastIndex()));
}

const itemstate::State& state() { return st().items; }

itemactions::Result ask(itemactions::Kind k, uint16_t index, uint32_t amount) {
    State& s = st();
    uint32_t have = 1;
    if (k != itemactions::Kind::CloseStorage) {
        const itemstate::Item* it = s.items.find(
            k == itemactions::Kind::ToStorage ? itemstate::Where::Bag : itemstate::Where::Storage, index);
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

void frame() {
    State& s = st();
    if (s.queue.tick(GetTickCount()) == itemactions::Outcome::TimedOut) {
        say(s, s.queue.lastKind() == itemactions::Kind::ToStorage ? settings::Text::ItemsStorageRefused
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
