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
    int levels() const { return int(images_.size()); }
    Image& level(int i) { return images_[size_t(i)]; }
    const Image& level(int i) const { return images_[size_t(i)]; }
    const ColorKey& key() const { return key_; }
    void setKey(const ColorKey& key);  // the alpha of keyed texels changes: every level is sent again
    void changed(int level, Rect r);   // r is clipped to the level
    bool pending() const;
    void sync(FrameRecord& record, uint32_t texture);  // records the changed rectangles as uploads

private:
    std::vector<Image> images_;
    std::vector<Rect> dirty_;
    ColorKey key_;
};

}  // namespace uagfx
