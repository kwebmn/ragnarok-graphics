#include "win/item_net.hpp"

#include <windows.h>

#include <algorithm>

#include "core/count_limit.hpp"
#include "core/item_packets.hpp"
#include "core/reference_index.hpp"
#include "core/settings/strings.hpp"
#include "pad/win/runtime.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/reference_load.hpp"
#include "win/words.hpp"

namespace uagfx::win::itemnet {
namespace {

constexpr int kBagPlaces = 100;  // 3.0.24-beta.22: the bag's (uAthena MAX_INVENTORY)

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
    bool vendWaiting = false;  // 3.0.24-beta.18: the deal waiting is our purchase at a player's shop
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
        case itemactions::Kind::Use: return "used";  // 3.0.24-beta.20: «Речі»'s own
        case itemactions::Kind::Equip: return "worn";
        case itemactions::Kind::Unequip: return "taken off";
        case itemactions::Kind::Drop: return "dropped";
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
    s.items.setSelf(uapad::runtime::selfAccount());  // 3.0.24-beta.20: the things used around us are not ours
    const std::optional<itemstate::Change> c = s.items.feed(p, n);
    if (!c) return;
    const itemactions::Outcome o = s.queue.onChange(*c);
    // 3.0.24-beta.18 (stage 4 «Лавка»): our purchase at a player's shop - the things coming into the bag end it, its
    // refusal (0x0135) says why
    if (s.vendWaiting && (o == itemactions::Outcome::Done || o == itemactions::Outcome::Refused)) {
        s.vendWaiting = false;
        s.dealAnswer = s.queue.lastResult();
        if (s.dealAnswer == 1) say(s, settings::Text::ItemsVendNoZeny);
        else if (s.dealAnswer == 2) say(s, settings::Text::ItemsVendTooHeavy);
        else if (s.dealAnswer != 0) say(s, settings::Text::ItemsVendRefused);
        if (s.told++ < 200)
            LOG_INFO("items: the shop's purchase %s (%d)", s.dealAnswer ? "refused" : "done - the things came",
                     s.dealAnswer);
        return;
    }
    if (c->where == itemstate::Where::Shop) {  // 3.0.24-beta.9: the deal answered
        if (o == itemactions::Outcome::None) return;  // not ours (the game's own window sent it)
        s.dealAnswer = c->result;
        if (c->result == 1) say(s, settings::Text::ItemsNoZeny);
        else if (c->result == 2) say(s, settings::Text::ItemsTooHeavy);
        else if (c->result != 0) say(s, settings::Text::ItemsShopRefused);
        if (s.told++ < 200) LOG_INFO("items: the NPC deal answered %d (%s)", c->result, c->result ? "refused" : "done");
        return;
    }
    if (o != itemactions::Outcome::None && s.told++ < 200)
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

itemactions::Result vend(uint32_t merchant, const itempackets::Pairs& rows) {
    State& s = st();
    itempackets::Bytes out;
    itemactions::Result r = s.queue.askVend(merchant, rows, uapad::runtime::healthy(), GetTickCount(), out);
    if (r == itemactions::Result::Sent && !uapad::runtime::sendPacket(out)) r = itemactions::Result::NotConnected;
    if (r == itemactions::Result::NotConnected) {
        s.queue.dropAll();
        say(s, settings::Text::ItemsNotConnected);
    }
    if (r == itemactions::Result::Sent) {
        s.dealAnswer = -2;
        s.vendWaiting = true;
    }
    if (s.told++ < 200)
        LOG_INFO("items: the shop's purchase (merchant %u) - %zu row(s): %s", unsigned(merchant), rows.size(),
                 r == itemactions::Result::Sent ? "sent" : r == itemactions::Result::Busy ? "the one before waits"
                                                                                           : "not sent");
    return r;
}

namespace {

// 3.0.24-beta.20: a bag thing's own action sent - its log line, the words when no connection
itemactions::Result sent(State& s, itemactions::Kind k, uint16_t index, itemactions::Result r,
                         const itempackets::Bytes& out) {
    if (r == itemactions::Result::Sent && !uapad::runtime::sendPacket(out)) {
        s.queue.dropAll();
        r = itemactions::Result::NotConnected;
    }
    if (r == itemactions::Result::NotConnected) say(s, settings::Text::ItemsNotConnected);
    if (s.told++ < 200)
        LOG_INFO("items: %s, index %u - %s", kindName(k), unsigned(index),
                 r == itemactions::Result::Sent           ? "sent"
                 : r == itemactions::Result::Busy         ? "waits for the one before"
                 : r == itemactions::Result::NotConnected ? "no map server connection (or our account not known)"
                                                          : "nothing there");
    return r;
}

}  // namespace

Fit fitIn(itemstate::Where to, int id, int count) {
    State& s = st();
    Fit f;
    if (count <= 0) return f;
    int unit = -1;  // one's weight in tenths (the reference pack's)
    if (const auto refs = reference::index()) {
        const auto w = refs->items.find(id);
        if (w != refs->items.end()) unit = w->second.weight;
    }
    const std::vector<itemstate::Item>& list = to == itemstate::Where::Bag    ? s.items.bag()
                                               : to == itemstate::Where::Cart ? s.items.cart()
                                                                              : s.items.storage();
    const bool stacks = std::any_of(list.begin(), list.end(),
                                    [id](const itemstate::Item& x) { return x.id == id && x.places == 0; });
    int weightFree = -1, placesFree = -1;
    settings::Text words = settings::Text::CountFitsStorage;
    if (to == itemstate::Where::Bag) {
        charstatus::Status st;
        if (readOwnStatus(st) && st.weightMost > 0) weightFree = (st.weightMost - st.weight) * 10;
        placesFree = std::max(0, kBagPlaces - int(list.size()));
        words = settings::Text::CountFitsBag;
    } else if (to == itemstate::Where::Cart) {
        if (s.items.cartMostWeight() > 0) weightFree = s.items.cartMostWeight() - s.items.cartWeight();  // tenths
        if (s.items.cartMost() > 0) placesFree = std::max(0, s.items.cartMost() - int(list.size()));
        words = settings::Text::CountFitsCart;
    } else if (s.items.storageMost() > 0) {
        placesFree = std::max(0, s.items.storageMost() - int(list.size()));
    }
    f.most = countlimit::most(count, unit, weightFree, placesFree, stacks);
    if (f.most < count) f.words = settings::fmt(win::say(words), {std::to_string(f.most)});
    return f;
}

itemactions::Result use(uint16_t index) {
    State& s = st();
    itempackets::Bytes out;
    const uint32_t account = uapad::runtime::selfAccount();
    const itemactions::Result r =
        s.queue.askUse(index, account, uapad::runtime::healthy() && account != 0, GetTickCount(), out);
    return sent(s, itemactions::Kind::Use, index, r, out);
}

bool openShop(const std::string& sign, const std::vector<itempackets::VendRow>& rows) {
    return uapad::runtime::healthy() && uapad::runtime::sendPacket(itempackets::openVending(sign, rows));
}

itemactions::Result equip(uint16_t index, uint16_t places) {
    State& s = st();
    itempackets::Bytes out;
    const itemactions::Result r = s.queue.askEquip(index, places, uapad::runtime::healthy(), GetTickCount(), out);
    return sent(s, itemactions::Kind::Equip, index, r, out);
}

itemactions::Result unequip(uint16_t index) {
    State& s = st();
    itempackets::Bytes out;
    const itemactions::Result r = s.queue.askUnequip(index, uapad::runtime::healthy(), GetTickCount(), out);
    return sent(s, itemactions::Kind::Unequip, index, r, out);
}

itemactions::Result drop(uint16_t index, uint32_t amount) {
    State& s = st();
    const itemstate::Item* it = s.items.find(itemstate::Where::Bag, index);
    itempackets::Bytes out;
    const itemactions::Result r = s.queue.askDrop(index, amount, it ? it->amount : amount,
                                                  uapad::runtime::healthy(), GetTickCount(), out);
    return sent(s, itemactions::Kind::Drop, index, r, out);
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
        s.vendWaiting = false;
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
