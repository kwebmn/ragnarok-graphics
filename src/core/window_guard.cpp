#include "core/window_guard.hpp"

#include <algorithm>

namespace uagfx::windowguard {

const Field kEnterFields[10] = {{0x70, 0, "the message box"},
                                {0xD8, 0x30, "the menu"},
                                {0x160, 0x38, "the number asked"},
                                {0x164, 0x64, "the words asked"},
                                {0xA8, 0x11, "the NPC's choices"},
                                {0xA4, 0x10, "the NPC's words"},
                                {0xC4, 0x1A, "window 0x1A"},
                                {0xBC, 0x27, "window 0x27"},
                                {0x19C, 0x4F, "window 0x4F"},
                                {0x1A0, 0x50, "window 0x50"}};

std::vector<uint32_t> listedWindows(const Read& read, uint32_t head, size_t most, bool& whole) {
    std::vector<uint32_t> out;
    whole = false;
    uint32_t node = 0;
    if (!head || !read(head, node)) return out;
    for (size_t seen = 0; node != head; ++seen) {
        uint32_t window = 0, next = 0;
        if (seen >= most || !node || !read(node + 8, window) || !read(node, next)) return out;
        out.push_back(window);
        node = next;
    }
    whole = true;
    return out;
}

std::vector<size_t> gone(const std::vector<uint32_t>& named, const std::vector<uint32_t>& listed) {
    std::vector<size_t> out;
    for (size_t i = 0; i < named.size(); ++i)
        if (named[i] && std::find(listed.begin(), listed.end(), named[i]) == listed.end()) out.push_back(i);
    return out;
}

}  // namespace uagfx::windowguard
