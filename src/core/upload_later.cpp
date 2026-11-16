#include "core/upload_later.hpp"

#include <utility>

#include "core/ai/texture_cache.hpp"
#include "core/effect_light.hpp"
#include "core/foliage.hpp"
#include "core/full_colour.hpp"

namespace uagfx::uploadlater {

void lookAt(Job&& j, bool aiKey, Looks& out) {
    out.texture = j.upload.texture;
    out.serial = j.serial;
    out.upload = j.upload;
    out.width = j.upload.rect.width();
    out.height = j.upload.rect.height();
    out.spriteCache = j.spriteCache;
    out.pixels = std::move(j.pixels);
    out.game.clear();
    out.aiKey.clear();
    const uint32_t* px = out.pixels.data();
    if (out.pixels.size() < size_t(out.width) * size_t(out.height) || !out.width || !out.height) {
        out.fire = out.light = 0.0f;
        out.leafy = false;
        return;
    }
    out.fire = effect_light::fireOf(px, out.width, out.height, out.fireColour);
    out.light = effect_light::lightOf(px, out.width, out.height, out.lightColour);
    out.leafy = foliage::leafyTexture(px, out.width, out.height, out.spriteCache);
    out.holes = foliage::holesPercent(px, out.width, out.height);  // 3.0.20-beta.61
    if (!aiKey) return;
    const uint32_t* texels = full_colour::aiTexels(out.upload, px, out.game);
    if (texels == px) out.game.clear();
    out.aiKey = ai::keyName(ai::textureKey(texels, out.width, out.height));
}

bool Queue::push(const UploadCmd& u, const uint32_t* pixels, bool spriteCache, uint32_t serial) {
    drop(u.texture);  // its older work: of pixels gone now
    const size_t n = size_t(u.rect.width()) * size_t(u.rect.height());
    if (!pixels || !n || bytes_ + n * 4 > kMostBytes) return false;
    Job j;
    j.upload = u;
    j.upload.pixels = 0;
    j.pixels.assign(pixels, pixels + n);
    j.spriteCache = spriteCache;
    j.serial = serial;
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
