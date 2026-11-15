#include "core/upload_later.hpp"

namespace uagfx::uploadlater {

bool Queue::push(const UploadCmd& u, const uint32_t* pixels, bool spriteCache) {
    drop(u.texture);  // its older work: of pixels gone now
    const size_t n = size_t(u.rect.width()) * size_t(u.rect.height());
    if (!pixels || !n || bytes_ + n * 4 > kMostBytes) return false;
    Job j;
    j.upload = u;
    j.upload.pixels = 0;
    j.pixels.assign(pixels, pixels + n);
    j.spriteCache = spriteCache;
    bytes_ += n * 4;
    jobs_.push_back(std::move(j));
    return true;
}

bool Queue::take(Job& out) {
    if (jobs_.empty()) return false;
    out = std::move(jobs_.front());
    jobs_.pop_front();
    bytes_ -= out.pixels.size() * 4;
    return true;
}

void Queue::drop(uint32_t texture) {
    for (auto it = jobs_.begin(); it != jobs_.end();) {
        if (it->upload.texture == texture) {
            bytes_ -= it->pixels.size() * 4;
            it = jobs_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace uagfx::uploadlater
