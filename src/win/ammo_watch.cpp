#include "win/ammo_watch.hpp"

#include <cstdint>
#include <cstdlib>

#include "core/char_block.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"

namespace uagfx::win::ammowatch {
namespace {

// The bag (win/item_window's: the session's list, a node's ITEM_INFO at +8 - its type +0, its place +0x08, how many
// +0x10, the item's number as text +0x30/+0x34)
constexpr uint32_t kBag = 0x771148 + 0xF1C, kInfo = 8;
// 3.0.11-beta.118: the arrows worn during the play are known by their place only (the session's +0xEEC, set by the
// server's 0x013c - at the login too); 3.0.11-beta.120: the login's worn 0x8000 not looked at (stale after a change)
constexpr uint32_t kAmmoNow = 0x771148 + 0xEEC;
constexpr size_t kInfoSize = 0x48, kMostNodes = 256;
constexpr ULONGLONG kReadMs = 500, kGoneMs = 30000;
constexpr int kArrowType = 10;  // the client's arrows; 16 bullets, 17 thrown things

struct State {
    Ammo ammo;
    bool lowTold = false;
    ULONGLONG readAt = 0, goneAt = 0;
};

State& state() {
    static State s;
    return s;
}

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

uint32_t rd32(const uint8_t* p, size_t at) {
    return uint32_t(p[at]) | uint32_t(p[at + 1]) << 8 | uint32_t(p[at + 2]) << 16 | uint32_t(p[at + 3]) << 24;
}

void forget(State& s) {
    s.ammo = Ammo{};
    s.goneAt = 0;
    s.lowTold = false;
}

void update(State& s, ULONGLONG now) {
    const uint32_t head = read32(kBag);
    if (!head) return;
    int count = -1, id = 0, type = 0;
    size_t things = 0;
    bool stillThere = false;
    const uint32_t ammo = read32(kAmmoNow);
    uint32_t node = read32(head);
    for (size_t n = 0; node && node != head && n < kMostNodes; ++n, node = read32(node)) {
        uint8_t raw[kInfoSize] = {};
        if (!readGame(node + kInfo, raw, sizeof raw)) return;
        const uint32_t ptr = rd32(raw, 0x30), len = rd32(raw, 0x34);
        char digits[17] = {};
        const int thing = ptr && len && len <= 16 && readGame(ptr, digits, len) ? std::atoi(digits) : 0;
        ++things;
        if (thing && thing == s.ammo.id) stillThere = true;
        if (ammo && rd32(raw, 0x08) == ammo) {  // 3.0.11-beta.120: by their place alone (the login's flag stays)
            count = int(rd32(raw, 0x10));
            id = thing;
            type = int(rd32(raw, 0x00));
        }
    }
    if (count >= 0) {
        if (id != s.ammo.id) {
            LOG_INFO("ammo: worn - #%d x%d (type %d)", id, count, type);
            s.lowTold = false;
        }
        if (count < charblock::kAmmoLow && !s.lowTold) {
            s.lowTold = true;
            LOG_INFO("ammo: running low - %d left: the hotbar's pill and the character block show it", count);
        }
        s.ammo.count = count;
        s.ammo.id = id;
        s.ammo.arrows = type == kArrowType || (type != 16 && type != 17);
        s.goneAt = 0;
        return;
    }
    if (!things) return;
    if (s.ammo.id && !stillThere) {
        if (!s.goneAt) {
            s.goneAt = now;
            LOG_INFO("ammo: #%d ran out (gone from the bag) - 0 shown for %d s", s.ammo.id, int(kGoneMs / 1000));
        }
        if (now - s.goneAt < kGoneMs) {
            s.ammo.count = 0;
            return;
        }
    }
    forget(s);
}

}  // namespace

const Ammo& read(ULONGLONG now) {
    State& s = state();
    if (!inGameWorld()) {
        forget(s);
        return s.ammo;
    }
    if (now - s.readAt >= kReadMs) {
        s.readAt = now;
        update(s, now);
    }
    return s.ammo;
}

}  // namespace uagfx::win::ammowatch
