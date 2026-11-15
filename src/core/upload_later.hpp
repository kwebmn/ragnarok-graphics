#pragma once
#include <cstddef>
#include <cstdint>
#include <deque>
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
};

class Queue {
public:
    bool push(const UploadCmd& u, const uint32_t* pixels, bool spriteCache);  // false: full - work on it now
    bool take(Job& out);                                                      // the oldest; false: none
    void drop(uint32_t texture);
    size_t size() const { return jobs_.size(); }
    size_t bytes() const { return bytes_; }

private:
    std::deque<Job> jobs_;
    size_t bytes_ = 0;
};

}  // namespace uagfx::uploadlater
