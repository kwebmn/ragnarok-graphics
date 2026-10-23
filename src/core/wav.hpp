#pragma once
#include <cstdint>
#include <cstring>

namespace uagfx::wav {

// 1.1: a sound file's image as the game loads it (data\wav\*.wav: RIFF WAVE) - its format and its samples.
struct Sound {
    uint16_t format = 0;  // 1 PCM
    uint16_t channels = 0;
    uint32_t rate = 0;
    uint16_t bits = 0;
    uint16_t blockAlign = 0;
    const uint8_t* data = nullptr;
    uint32_t bytes = 0;
    const uint8_t* fmt = nullptr;  // 3.0.1-beta.7: its fmt chunk's body (ADPCM's own fields after the 16 bytes)
    uint32_t fmtBytes = 0;
};

// The image at `p`, at most `most` bytes (its own RIFF size says how many): false for anything but a WAVE with a
// format and samples. Its chunks are read in order, an odd chunk padded to two bytes.
inline bool parse(const uint8_t* p, uint32_t most, Sound& out) {
    if (!p || most < 12 || std::memcmp(p, "RIFF", 4) != 0 || std::memcmp(p + 8, "WAVE", 4) != 0) return false;
    uint32_t riff = 0;
    std::memcpy(&riff, p + 4, 4);
    const uint32_t end = riff + 8 < most ? riff + 8 : most;
    bool format = false;
    for (uint32_t at = 12; at + 8 <= end;) {
        uint32_t size = 0;
        std::memcpy(&size, p + at + 4, 4);
        const uint8_t* body = p + at + 8;
        const uint32_t room = end - (at + 8);
        if (std::memcmp(p + at, "fmt ", 4) == 0 && size >= 16 && size <= room) {
            std::memcpy(&out.format, body, 2);
            std::memcpy(&out.channels, body + 2, 2);
            std::memcpy(&out.rate, body + 4, 4);
            std::memcpy(&out.blockAlign, body + 12, 2);
            std::memcpy(&out.bits, body + 14, 2);
            out.fmt = body;
            out.fmtBytes = size;
            format = true;
        } else if (std::memcmp(p + at, "data", 4) == 0) {
            out.data = body;
            out.bytes = size <= room ? size : room;  // a data chunk cut short: what there is
            return format && out.channels > 0 && out.rate > 0 && out.blockAlign > 0;
        }
        if (size > room) break;
        at += 8 + size + (size & 1);
    }
    return false;
}

// Whether the new engine plays it itself: plain PCM, 8 or 16 bits, mono or stereo, at a rate it takes.
inline bool playable(const Sound& s) {
    return s.format == 1 && (s.bits == 8 || s.bits == 16) && (s.channels == 1 || s.channels == 2) &&
           s.rate >= 1000 && s.rate <= 192000 && s.blockAlign == s.channels * s.bits / 8 && s.bytes >= s.blockAlign;
}

}  // namespace uagfx::wav
