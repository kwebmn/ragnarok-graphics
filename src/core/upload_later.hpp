#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "core/frame.hpp"

// 3.0.20-beta.20 (the owner's «делай 1 и 2», 1 - R-U7QZPV/R-DHG86V: the long frames' dearest part, textures' uploads
// 16-27 ms; a picture's new textures each read six or seven times on the renderer's thread - fire, light, leaves, the
// AI pack's copies and key): a whole texture's pixels kept for that work after its upload, which goes to the card at
// once - the renderers take it a picture's budget at a time, in the order the uploads came; a newer upload of the same
// texture takes the older one's place, a texture let go takes its work with it; kMostBytes kept at most (the caller
// works on one past it at once)
namespace uagfx::uploadlater {

constexpr size_t kMostBytes = 64u << 20;

struct Job {
    UploadCmd upload;  // its texture, rect, level, formats; its pixels from 0 of `pixels`
    std::vector<uint32_t> pixels;
    bool spriteCache = false;
    uint32_t serial = 0;  // 3.0.20-beta.36: its upload's (the renderer's count) - a newer one's work goes in alone
};

// 3.0.20-beta.36 (the owner's «6,7,8,9», 9 - the frame's work on more threads): what the work makes of a job - its
// looks (fire, light, leaves) and, with the AI pack, the pack's view of it and its key, all from the job alone (a
// helper thread's); its pixels kept for the pack, which takes them on the renderer's thread
struct Looks {
    uint32_t texture = 0, serial = 0;
    UploadCmd upload;
    int width = 0, height = 0;
    bool spriteCache = false;
    float fire = 0.0f, fireColour[3] = {}, light = 0.0f, lightColour[3] = {};
    bool leafy = false;
    int holes = 0;  // 3.0.20-beta.61: foliage::holesPercent
    std::vector<uint32_t> pixels;
    std::vector<uint32_t> game;  // the pack's view of a 16-bit texture's full colours; empty: the pixels themselves
    std::string aiKey;           // ai::keyName of its texels; empty without the pack
    const uint32_t* texels() const { return game.empty() ? pixels.data() : game.data(); }
};
void lookAt(Job&& j, bool aiKey, Looks& out);
// 3.0.20-beta.45 (the audit): whether a texture takes the looks made of its upload `serial` - `first` its first whole
// upload's number (0: never written whole), `last` its newest. Any upload of this texture's own, a write behind too:
// beta.36 took the newest alone, and a texture the game writes every frame (its looks a picture late each time)
// never got any; not one of the texture that had its number before it
inline bool takes(uint32_t first, uint32_t last, uint32_t serial) {
    return first != 0 && serial >= first && serial <= last;
}

class Queue {
public:
    // false: full - work on it now
    bool push(const UploadCmd& u, const uint32_t* pixels, bool spriteCache, uint32_t serial = 0);
    bool take(Job& out);                                                      // the oldest; false: none
    void drop(uint32_t texture);
    size_t size() const { return jobs_.size(); }
    size_t bytes() const { return bytes_; }

private:
    std::deque<Job> jobs_;
    size_t bytes_ = 0;
};

}  // namespace uagfx::uploadlater
