// 3.0.11-beta.194 (R-ZS5B2D: uaRO.exe crashed on Enter 17 s into geffen_in - the windows' manager still named its
// menu, a window already freed): the windows the manager's list has and the fields naming one it has not
#include <cstdint>
#include <map>
#include <vector>

#include "core/window_guard.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// The game's memory as a map of 32-bit values; an address not in it not readable
windowguard::Read reader(const std::map<uint32_t, uint32_t>& memory) {
    return [&memory](uint32_t at, uint32_t& value) {
        const auto it = memory.find(at);
        if (it == memory.end()) return false;
        value = it->second;
        return true;
    };
}

// A ring of nodes {next, previous, window} from its head node (no window of its own), one node a window
std::map<uint32_t, uint32_t> ring(uint32_t head, const std::vector<uint32_t>& windows) {
    std::map<uint32_t, uint32_t> m;
    std::vector<uint32_t> nodes = {head};
    for (size_t i = 0; i < windows.size(); ++i) nodes.push_back(0x2000 + uint32_t(i) * 0x10);
    for (size_t i = 0; i < nodes.size(); ++i) {
        m[nodes[i]] = nodes[(i + 1) % nodes.size()];
        m[nodes[i] + 4] = nodes[(i + nodes.size() - 1) % nodes.size()];
        m[nodes[i] + 8] = i ? windows[i - 1] : 0;
    }
    return m;
}

}  // namespace

TEST(window_guard_lists_the_ring) {
    const auto memory = ring(0x1000, {0xA000, 0xB000, 0xC000});
    bool whole = false;
    const auto listed = windowguard::listedWindows(reader(memory), 0x1000, 64, whole);
    CHECK(whole);
    CHECK(listed == (std::vector<uint32_t>{0xA000, 0xB000, 0xC000}));
    const auto empty = ring(0x1000, {});
    CHECK(windowguard::listedWindows(reader(empty), 0x1000, 64, whole).empty() && whole);
}

// Nothing to judge by: a node not readable, a ring longer than the most looked at, no head
TEST(window_guard_judges_only_a_whole_ring) {
    auto memory = ring(0x1000, {0xA000, 0xB000, 0xC000});
    bool whole = true;
    windowguard::listedWindows(reader(memory), 0x1000, 2, whole);
    CHECK(!whole);
    memory.erase(0x2010);  // the second window's node: its next not readable
    windowguard::listedWindows(reader(memory), 0x1000, 64, whole);
    CHECK(!whole);
    whole = true;
    windowguard::listedWindows(reader(memory), 0, 64, whole);
    CHECK(!whole);
}

// The fields naming a window the list has not - none named (0) is not one
TEST(window_guard_finds_the_gone_ones) {
    const std::vector<uint32_t> listed = {0xA000, 0xB000, 0xC000};
    CHECK(windowguard::gone({0, 0xB000, 0x1686DCD0, 0, 0xA000}, listed) == (std::vector<size_t>{2}));
    CHECK(windowguard::gone({0, 0, 0}, listed).empty());
    CHECK(windowguard::gone({0xC000}, {}) == (std::vector<size_t>{0}));
}

// Enter's fields (0x4CE990) in its order: the message box first, the menu (UI window 0x30) second - not the focused
// control (+0x6C), a part of a window
TEST(window_guard_enter_fields) {
    const auto& f = windowguard::kEnterFields;
    const uint32_t offsets[] = {0x70, 0xD8, 0x160, 0x164, 0xA8, 0xA4, 0xC4, 0xBC, 0x19C, 0x1A0};
    CHECK_EQ(sizeof f / sizeof f[0], sizeof offsets / sizeof offsets[0]);
    for (size_t i = 0; i < sizeof offsets / sizeof offsets[0]; ++i) CHECK_EQ(f[i].offset, offsets[i]);
    CHECK_EQ(f[1].window, 0x30);
    CHECK_EQ(f[0].window, 0);
}
