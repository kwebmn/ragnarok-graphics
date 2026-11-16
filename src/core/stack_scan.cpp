#include "core/stack_scan.hpp"

#include <cstdio>

namespace uagfx::stackscan {

namespace {

int moduleOf(uint64_t address, const std::vector<Module>& modules) {
    for (size_t i = 0; i < modules.size(); ++i)
        if (address >= modules[i].base && address - modules[i].base < modules[i].size) return int(i);
    return -1;
}

std::string hex(uint64_t v) {
    char text[24];
    std::snprintf(text, sizeof text, "0x%llx", static_cast<unsigned long long>(v));
    return text;
}

}  // namespace

bool afterCall(uint64_t at, ReadFn read, void* context) {
    uint8_t b[7] = {};
    if (!read || at < 7) return false;
    // the longest form first read whole; a shorter one's bytes are its last
    const bool all = read(context, at - 7, b, 7);
    if (!all) {  // the image's very start: what can be read of it
        if (!read(context, at - 5, b + 2, 5)) return false;
        b[0] = b[1] = 0;
    }
    if (b[2] == 0xE8) return true;                                // call rel32
    if (b[1] == 0xFF && b[2] == 0x15) return true;                // call [rip+disp32]
    if (b[5] == 0xFF && (b[6] & 0xF8) == 0xD0) return true;       // call reg
    if (b[5] == 0xFF && (b[6] & 0xF8) == 0x10 && (b[6] & 7) != 4 && (b[6] & 7) != 5) return true;  // call [reg]
    if (b[4] == 0xFF && (b[5] & 0xF8) == 0x50 && (b[5] & 7) != 4) return true;                     // call [reg+d8]
    if (b[3] == 0xFF && b[4] == 0x54) return true;                // call [sib+d8]
    if (b[1] == 0xFF && (b[2] & 0xF8) == 0x90 && (b[2] & 7) != 4) return true;                     // call [reg+d32]
    if (b[0] == 0xFF && b[1] == 0x94) return true;                // call [sib+d32]
    return false;
}

std::vector<Frame> scan(const uint64_t* words, size_t count, const std::vector<Module>& modules, ReadFn read,
                        void* context, size_t most) {
    std::vector<Frame> out;
    for (size_t i = 0; words && i < count && out.size() < most; ++i) {
        const int m = moduleOf(words[i], modules);
        if (m < 0 || !afterCall(words[i], read, context)) continue;
        out.push_back(Frame{m, words[i] - modules[size_t(m)].base});
    }
    return out;
}

std::string place(uint64_t address, const std::vector<Module>& modules) {
    const int m = moduleOf(address, modules);
    return m < 0 ? hex(address) : modules[size_t(m)].name + "+" + hex(address - modules[size_t(m)].base);
}

std::string calls(const std::vector<Frame>& frames, const std::vector<Module>& modules) {
    std::string text;
    for (const Frame& f : frames) {
        if (f.module < 0 || size_t(f.module) >= modules.size()) continue;
        if (!text.empty()) text += " < ";
        text += modules[size_t(f.module)].name + "+" + hex(f.rva);
    }
    return text.empty() ? "none read" : text;
}

}  // namespace uagfx::stackscan
