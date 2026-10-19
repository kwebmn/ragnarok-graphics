#pragma once
#include <cstdint>
#include <vector>

#include "core/frame.hpp"
#include "core/image.hpp"
#include "core/surface_ops.hpp"

namespace uagfx {

// The CPU side of a texture and its mip levels: the client writes them with Lock and Blt and reads them back for
// the GDI path (surfaces-present-caps.md §2, L2155). The GPU texture gets the changed rectangles just before a
// draw samples it, so many small writes between two draws cost one upload. A new texture counts as changed
// everywhere: its memory is zeroed and the GPU texture starts undefined.
class TextureLevels {
public:
    TextureLevels(int width, int height, PixelFormat format, int levels);
    ~TextureLevels();
    TextureLevels(const TextureLevels&) = delete;
    TextureLevels& operator=(const TextureLevels&) = delete;
    int levels() const { return int(images_.size()); }
    Image& level(int i) { return images_[size_t(i)]; }
    const Image& level(int i) const { return images_[size_t(i)]; }
    const ColorKey& key() const { return key_; }
    void setKey(const ColorKey& key);  // the alpha of keyed texels changes: every level is sent again
    void changed(int level, Rect r);   // r is clipped to the level
    // AI textures (0.13): a cell of the game's sprite cache got a new frame (of `source`, sprite_exe::Source); the
    // next upload of level 0 names it. 2.5.23: own - the frame in its sprite's own colours when the game wrote it in
    // a dye (the cell's size; else none).
    void slotWritten(Rect r, uint8_t source = 0, const std::vector<uint32_t>* own = nullptr);
    bool pending() const;
    void sync(FrameRecord& record, uint32_t texture);  // records the changed rectangles as uploads
    // 2.5.18: the game's own texels of level 0 before it made them 16-bit (core/full_colour.hpp): r of level 0, argb
    // r.width() * r.height() of them; mips - it made its mip levels of level 0 again. The next upload sends them in
    // full colour where the 16-bit texels are still theirs, the mip levels made of them; false: none kept - no 16-bit
    // format, r not inside level 0, or the full colours of all textures waiting for their uploads at kFullBytes.
    bool fullColour(const Rect& r, const uint32_t* argb, bool mips);
    static size_t fullBytes();
    static constexpr size_t kFullBytes = size_t(96) << 20;  // the game's address space is 32-bit

private:
    bool mergeFull(const Rect& r, std::vector<uint32_t>& argb) const;  // level 0's texels of r, from full_
    void dropFull();

    std::vector<Image> images_;
    std::vector<Rect> dirty_;
    std::vector<SpriteCell> slots_;  // since the last upload of level 0
    std::vector<std::vector<uint32_t>> slotOwn_;  // 2.5.23: each one's own colours (empty: none)
    ColorKey key_;
    std::vector<uint32_t> full_;  // 2.5.18: level 0's full colours (its size) while some wait for their upload
    Rect fullRect_;               // where they are
    bool fullMips_ = false;       // the mip levels to make of them
};

}  // namespace uagfx
