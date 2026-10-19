#include "core/texture_levels.hpp"

#include <algorithm>
#include <atomic>

#include "core/full_colour.hpp"

namespace uagfx {
namespace {

std::atomic<size_t> g_fullBytes{0};  // 2.5.18: every texture's full_

Rect unite(const Rect& a, const Rect& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    return Rect{std::min(a.left, b.left), std::min(a.top, b.top), std::max(a.right, b.right),
                std::max(a.bottom, b.bottom)};
}

}  // namespace

TextureLevels::TextureLevels(int width, int height, PixelFormat format, int levels) {
    for (int i = 0; i < std::max(1, levels); ++i) {
        images_.emplace_back(std::max(1, width >> i), std::max(1, height >> i), format);
        dirty_.push_back(images_.back().bounds());
    }
}

TextureLevels::~TextureLevels() { dropFull(); }

size_t TextureLevels::fullBytes() { return g_fullBytes.load(); }

void TextureLevels::dropFull() {
    if (!full_.empty()) g_fullBytes -= full_.size() * 4;
    std::vector<uint32_t>().swap(full_);
    fullRect_ = Rect{};
    fullMips_ = false;
}

bool TextureLevels::fullColour(const Rect& r, const uint32_t* argb, bool mips) {
    if (images_.empty() || !argb || !full_colour::shortFormat(images_[0].format())) return false;
    Rect inside = r;
    if (!clipRect(images_[0].bounds(), inside) || !(inside == r)) return false;
    const size_t w0 = size_t(images_[0].width()), all = w0 * size_t(images_[0].height());
    if (full_.empty()) {
        if (g_fullBytes.load() + all * 4 > kFullBytes) return false;
        full_.assign(all, 0u);
        g_fullBytes += all * 4;
    }
    for (int y = 0; y < r.height(); ++y)
        std::copy(argb + size_t(y) * size_t(r.width()), argb + size_t(y + 1) * size_t(r.width()),
                  full_.begin() + std::ptrdiff_t((size_t(r.top + y)) * w0 + size_t(r.left)));
    fullRect_ = unite(fullRect_, r);
    fullMips_ = fullMips_ || mips;
    changed(0, r);  // already, unless an upload came between the game's write and this
    return true;
}

bool TextureLevels::mergeFull(const Rect& r, std::vector<uint32_t>& argb) const {
    Rect m = r;
    if (full_.empty() || !clipRect(fullRect_, m)) return false;
    const Image& img = images_[0];
    const size_t w0 = size_t(img.width());
    for (int y = m.top; y < m.bottom; ++y)
        for (int x = m.left; x < m.right; ++x) {
            uint32_t& t = argb[size_t(y - r.top) * size_t(r.width()) + size_t(x - r.left)];
            t = full_colour::merge(img.format(), img.pixel(x, y), t, full_[size_t(y) * w0 + size_t(x)]);
        }
    return true;
}

void TextureLevels::setKey(const ColorKey& key) {
    key_ = key;
    for (size_t i = 0; i < images_.size(); ++i) dirty_[i] = images_[i].bounds();
}

void TextureLevels::changed(int level, Rect r) {
    if (level < 0 || level >= levels() || !clipRect(images_[size_t(level)].bounds(), r)) return;
    dirty_[size_t(level)] = unite(dirty_[size_t(level)], r);
}

void TextureLevels::slotWritten(Rect r, uint8_t source, const std::vector<uint32_t>* own) {
    const Rect asked = r;
    if (levels() == 0 || !clipRect(images_[0].bounds(), r)) return;
    const bool whole = own && r == asked && own->size() == size_t(r.width()) * size_t(r.height());
    for (size_t i = 0; i < slots_.size(); ++i)
        if (slots_[i].rect == r) {  // written again before the upload: the cell's newest frame goes once
            slots_[i].source = source;
            slotOwn_[i] = whole ? *own : std::vector<uint32_t>();
            return;
        }
    slots_.push_back(SpriteCell{r, source});
    slotOwn_.push_back(whole ? *own : std::vector<uint32_t>());
}

bool TextureLevels::pending() const {
    for (const Rect& r : dirty_)
        if (!r.empty()) return true;
    return false;
}

void TextureLevels::sync(FrameRecord& record, uint32_t texture) {
    std::vector<uint32_t> argb;
    const PixelFormat f = images_.empty() ? PixelFormat::Unknown : images_[0].format();
    // 2.5.18: the levels made of the full colours, from level 0 down as far as the changed mip levels need
    std::vector<uint32_t> chain, next;
    int chainLevel = -1, cw = 0, ch = 0;
    bool sent = false;
    for (size_t i = 0; i < images_.size(); ++i) {
        if (dirty_[i].empty()) continue;
        const Rect d = dirty_[i];
        toArgbRect(images_[i], d, key_, argb);
        bool full = false;
        if (i == 0) {
            full = mergeFull(d, argb);
        } else if (fullMips_ && !full_.empty()) {
            try {  // the game's thread, its address space: without memory for them the mips go up 16-bit
                if (chainLevel < 0) {
                    cw = images_[0].width();
                    ch = images_[0].height();
                    toArgbRect(images_[0], images_[0].bounds(), key_, chain);
                    mergeFull(images_[0].bounds(), chain);
                    chainLevel = 0;
                }
                for (; chainLevel < int(i); ++chainLevel) {
                    int nw = 0, nh = 0;
                    full_colour::halve(chain, cw, ch, next, nw, nh);
                    chain.swap(next);
                    cw = nw;
                    ch = nh;
                }
                for (int y = d.top; y < d.bottom; ++y)
                    for (int x = d.left; x < d.right; ++x) {
                        uint32_t& t = argb[size_t(y - d.top) * size_t(d.width()) + size_t(x - d.left)];
                        t = full_colour::mergeMip(f, t, chain[size_t(y) * size_t(cw) + size_t(x)]);
                    }
                full = true;
            } catch (...) {
                fullMips_ = false;
            }
        }
        record.upload(texture, uint32_t(i), d, argb.data(), i == 0 ? slots_ : std::vector<SpriteCell>(),
                      full ? f : PixelFormat::Unknown,
                      i == 0 ? slotOwn_ : std::vector<std::vector<uint32_t>>());
        dirty_[i] = Rect{};
        if (i == 0) {
            slots_.clear();
            slotOwn_.clear();
        }
        sent = true;
    }
    if (sent) dropFull();
}

}  // namespace uagfx
