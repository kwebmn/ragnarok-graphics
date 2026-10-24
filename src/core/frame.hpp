#pragma once
#include <cstdint>
#include <vector>

#include "core/image.hpp"
#include "core/states.hpp"

namespace uagfx {

// Everything the client drew between two flushes, in order, as plain data: no pointers, so a record can be
// executed here, written to a capture file, or (later) sent to a 64-bit renderer process.
enum class Cmd : uint8_t { Clear, Viewport, Draw, Upload, WriteTarget };

struct ClearCmd {
    uint32_t flags = 0;  // d3d7::CLEAR_TARGET | CLEAR_ZBUFFER
    uint32_t argb = 0;
    float z = 1.0f;
    Rect rect;
};

struct ViewportCmd {
    Rect rect;
    float minZ = 0.0f;
    float maxZ = 1.0f;
};

struct DrawCmd {
    uint32_t state = 0;            // index into states()
    uint32_t texture[2] = {0, 0};  // renderer texture ids; 0 = nothing bound
    bool keyed[2] = {false, false};  // the texture has a colour key baked into alpha 0
    // 0.28: the game's effect the draw is of (effect_exe::Kind; 0 none) and a seed of its primitive's own.
    uint8_t effect = 0;
    uint8_t effectSeed = 0;
    uint16_t effectId = 0;  // 2.7.21: the game's own number of that effect (effect_exe::Tag::id; 0 none)
    uint32_t primitive = 0;        // d3d7::PT_*
    uint32_t fvf = 0;              // d3d7::FVF_TL1 or FVF_TL2
    uint32_t vertexOffset = 0;     // bytes into vertexBytes()
    uint32_t vertexCount = 0;
    uint32_t firstIndex = 0;       // into indices()
    uint32_t indexCount = 0;       // 0 = not indexed
    // Stage 6: a tile of a game window picture drawn from the picture's twin (win/crisp) - the twin texture, the
    // tile's place in the picture in game pixels and the scale K the twin was made at; twin 0 = none.
    uint32_t twin = 0;
    int32_t twinTile[2] = {0, 0};
    double twinScale = 0.0;
};

// AI textures (0.13): a cell of the game's sprite cache that got a new frame - the whole cell, the frame in its top
// left corner - and (0.16) what sprite the frame is of (sprite_exe::Source).
struct SpriteCell {
    Rect rect;
    uint8_t source = 0;
    // 2.5.23: a frame the game wrote in a dye (another palette) - the same frame in its sprite's own colours at
    // pixels()[own - 1], the cell's size: the AI textures find the pack's picture by it and colour it as the dye; 0
    // none
    uint32_t own = 0;
    bool operator==(const SpriteCell& o) const { return rect == o.rect && source == o.source; }
};

// 0.32: a sprite cache's cells written again - what each holds now (at most 1024 kept); 2.2.2: a cell of another size
// over older ones (the texture taken back for frames of another size) puts them out.
void spriteCellsWritten(std::vector<SpriteCell>& cells, const SpriteCell* slots, uint32_t count);
// What the cell of a sprite cache (width x height texels) at the texture coordinates (u, v) holds - its source; 0
// (unknown) outside every cell.
uint8_t spriteSourceAt(const std::vector<SpriteCell>& cells, int width, int height, float u, float v);
// 2.2.2: what a draw of the cache shows - the cell at the middle of the first texture coordinates of its `count`
// vertices (u, v 24 bytes into a transformed vertex) from bytes[offset]; 0 when they are not all there.
uint8_t spriteSourceOf(const std::vector<SpriteCell>& cells, int width, int height, const std::vector<uint8_t>& bytes,
                       size_t offset, size_t stride, uint32_t count);

struct UploadCmd {  // texture pixels, ARGB8, rect.width() * rect.height() values at pixels()[pixels]
    uint32_t texture = 0;
    uint32_t level = 0;
    Rect rect;
    uint32_t pixels = 0;
    // AI textures (0.13): the cells of the game's sprite cache that got a new frame since the last upload, at
    // slots()[firstSlot, firstSlot + slotCount).
    uint32_t firstSlot = 0;
    uint32_t slotCount = 0;
    // 2.5.18: the game's 16-bit format (a PixelFormat) when the pixels hold its texture's own full colours
    // (core/full_colour.hpp) - the AI textures take the texture by its 16-bit texels; 0: as the texture has them.
    uint8_t fullFrom = 0;
};

struct WriteTargetCmd {  // CPU pixels written into the render target (Unlock after a write, Blt from memory)
    Rect rect;
    uint32_t pixels = 0;
};

struct Command {
    Cmd type;
    uint32_t index;  // into the vector of that type
};

class FrameRecord;
namespace bridge {
class WireReader;
bool readRecord(WireReader& rd, FrameRecord& r);
}  // namespace bridge

class FrameRecord {
public:
    static uint32_t stride(uint32_t fvf);

    uint32_t addState(const DrawSetup& s);
    void clear(const ClearCmd& c);
    void viewport(const ViewportCmd& v);
    void draw(DrawCmd cmd, const void* vertices, uint32_t vertexCount, const uint16_t* indices, uint32_t indexCount);
    // own (2.5.23): for slots[i] the frame in its own colours (the cell's size), or none (empty)
    void upload(uint32_t texture, uint32_t level, const Rect& rect, const uint32_t* argb,
                const std::vector<SpriteCell>& slots = {}, PixelFormat fullFrom = PixelFormat::Unknown,
                const std::vector<std::vector<uint32_t>>& own = {});
    void writeTarget(const Rect& rect, const uint32_t* argb);
    bool empty() const { return commands_.empty(); }
    // 3.0.3-beta.8: the draws, clears and writes since the last call, through flushes (reset keeps it): whether the
    // game drew a picture anew - the windows' panel goes over such a one alone (the target keeps its pixels)
    uint32_t takeWork() {
        const uint32_t n = work_;
        work_ = 0;
        return n;
    }
    // Bytes waiting to go to the GPU: vertices, indices and pixels. Past kFlushBytes the driver plays the record at
    // once rather than letting the GPU upload buffers grow: a 32-bit game has 2 GB of address space.
    static constexpr size_t kFlushBytes = size_t(16) << 20;
    size_t pendingBytes() const {
        return vertices_.size() + indices_.size() * sizeof(uint16_t) + pixels_.size() * sizeof(uint32_t);
    }
    bool full() const { return pendingBytes() >= kFlushBytes; }
    void reset();

    const std::vector<Command>& commands() const { return commands_; }
    const std::vector<ClearCmd>& clears() const { return clears_; }
    const std::vector<ViewportCmd>& viewports() const { return viewports_; }
    const std::vector<DrawCmd>& draws() const { return draws_; }
    const std::vector<UploadCmd>& uploads() const { return uploads_; }
    const std::vector<WriteTargetCmd>& writes() const { return writes_; }
    const std::vector<DrawSetup>& states() const { return states_; }
    const std::vector<uint8_t>& vertexBytes() const { return vertices_; }
    const std::vector<uint16_t>& indices() const { return indices_; }
    const std::vector<uint32_t>& pixels() const { return pixels_; }
    const std::vector<SpriteCell>& slots() const { return slots_; }

private:
    friend bool bridge::readRecord(bridge::WireReader& rd, FrameRecord& r);  // part 3: a record from its bytes
    uint32_t addPixels(const uint32_t* argb, size_t count);

    std::vector<Command> commands_;
    std::vector<ClearCmd> clears_;
    std::vector<ViewportCmd> viewports_;
    std::vector<DrawCmd> draws_;
    std::vector<UploadCmd> uploads_;
    std::vector<WriteTargetCmd> writes_;
    std::vector<DrawSetup> states_;
    std::vector<uint8_t> vertices_;
    std::vector<uint16_t> indices_;
    std::vector<uint32_t> pixels_;
    uint32_t work_ = 0;
    std::vector<SpriteCell> slots_;
};

}  // namespace uagfx
