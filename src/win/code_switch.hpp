#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::win {

// 3.0.13-beta.8: a few bytes of uaRO.exe's code switched between the game's own and ours as a setting wants - checked
// first (either form there, else another build: left alone, a log line once); written only when the wish changes
struct CodeSwitch {
    const char* what;      // the log's words
    uint32_t at;           // the address in uaRO.exe
    const uint8_t* game;   // the game's bytes
    const uint8_t* ours;   // ours
    size_t size;
    int now = -1;          // -1 not looked at, 0 the game's, 1 ours
    bool known = true;
    void want(bool ours);
};

}  // namespace uagfx::win
