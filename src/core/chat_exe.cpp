#include "core/chat_exe.hpp"

namespace uagfx::chat_exe {

const uint8_t kOriginal[kSize] = {
    0x85, 0xC0,        // test eax, eax
    0x7D, 0x0F,        // jge kKeep
    0x83, 0xF8, 0x0A,  // cmp eax, 10
    0x7E, 0x0A,        // jle kKeep
};

const uint8_t kFixed[kSize] = {
    0x83, 0xF8, 0x0A,        // cmp eax, 10
    0x76, 0x0E,              // jbe kKeep: 0 to 10 kept, the rest (a negative one too) starts over
    0x90, 0x90, 0x90, 0x90,  // nop
};

}  // namespace uagfx::chat_exe
