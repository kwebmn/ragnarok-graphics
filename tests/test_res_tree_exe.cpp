// 3.0.23-beta.4 (R-WS4ETW, R-NE7JVF: a read at 0 in uaRO.exe+0x112783/0x112789 at malangdo's entry, three times, the
// dump's stack the map loader thread's in CResMgr::Get): the resource cache's tree walked as the game's own
// lower_bound (0x517160) does - a node whose key is not written yet (another thread inserting it) or no key asked:
// not found, not a fault
#include <cstdint>
#include <map>

#include "core/res_tree_exe.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
// a tree of nodes: +0 left, +4 parent, +8 right, +0xc the key's address; the map's +4 its head, the head's +4 the root
struct Fake {
    std::map<uint32_t, uint32_t> mem;
    uint32_t read(uint32_t a) const {
        const auto it = mem.find(a);
        return it == mem.end() ? 0 : it->second;
    }
};
constexpr uint32_t kNil = 0x9000, kMap = 0x100, kHead = 0x200;
void node(Fake& f, uint32_t at, uint32_t left, uint32_t right, uint32_t key) {
    f.mem[at] = left, f.mem[at + 8] = right, f.mem[at + 0xc] = key;
}
}  // namespace

TEST(res_tree_lower_bound_as_the_game_and_safe) {
    Fake f;
    f.mem[kMap + 4] = kHead;
    f.mem[kHead + 4] = 0x300;           // the root
    node(f, 0x300, 0x400, 0x500, 50);   // keys are their own values here: less(a, b) = a < b
    node(f, 0x400, kNil, kNil, 20);
    node(f, 0x500, kNil, kNil, 80);
    f.mem[0x700] = 60;  // the key asked: 60 at 0x700 (keyRef points at the key's address)
    f.mem[0x710] = 0x700;
    const auto read = [&](uint32_t a) { return f.read(a); };
    const auto less = [&](uint32_t a, uint32_t b) { return f.read(a) < f.read(b); };
    // keys by address: node keys point at cells holding their values
    f.mem[0x300 + 0xc] = 0x800, f.mem[0x800] = 50;
    f.mem[0x400 + 0xc] = 0x810, f.mem[0x810] = 20;
    f.mem[0x500 + 0xc] = 0x820, f.mem[0x820] = 80;
    bool broken = false;
    CHECK_EQ(res_tree_exe::lowerBound(kMap, 0x710, kNil, read, less, broken), 0x500u);  // the first not less than 60
    CHECK(!broken);
    f.mem[0x500 + 0xc] = 0;  // its key not written yet
    CHECK_EQ(res_tree_exe::lowerBound(kMap, 0x710, kNil, read, less, broken), kHead);  // not found
    CHECK(broken);
    broken = false;
    f.mem[0x500 + 0xc] = 0x820;
    f.mem[0x710] = 0;  // no key asked
    CHECK_EQ(res_tree_exe::lowerBound(kMap, 0x710, kNil, read, less, broken), kHead);
    CHECK(broken);
}

TEST(res_tree_call_redirected) {
    uint8_t call[5];
    res_tree_exe::callTo(0x513799, 0x10000000, call);
    CHECK_EQ(call[0], 0xE8);
    const uint32_t rel = uint32_t(call[1]) | uint32_t(call[2]) << 8 | uint32_t(call[3]) << 16 | uint32_t(call[4]) << 24;
    CHECK_EQ(0x513799u + 5u + rel, 0x10000000u);
    CHECK_EQ(res_tree_exe::kCalls[0], 0x513799u);
    CHECK_EQ(res_tree_exe::kCalls[1], 0x5142D9u);
}
