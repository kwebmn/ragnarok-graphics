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
    uint32_t primitive = 0;        // d3d7::PT_*
    uint32_t fvf = 0;              // d3d7::FVF_TL1 or FVF_TL2
    uint32_t vertexOffset = 0;     // bytes into vertexBytes()
    uint32_t vertexCount = 0;
    uint32_t firstIndex = 0;       // into indices()
    uint32_t indexCount = 0;       // 0 = not indexed
};

struct UploadCmd {  // texture pixels, ARGB8, rect.width() * rect.height() values at pixels()[pixels]
    uint32_t texture = 0;
    uint32_t level = 0;
    Rect rect;
    uint32_t pixels = 0;
};

struct WriteTargetCmd {  // CPU pixels written into the render target (Unlock after a write, Blt from memory)
    Rect rect;
    uint32_t pixels = 0;
};

struct Command {
    Cmd type;
    uint32_t index;  // into the vector of that type
};

class FrameRecord {
public:
    static uint32_t stride(uint32_t fvf);

    uint32_t addState(const DrawSetup& s);
    void clear(const ClearCmd& c);
    void viewport(const ViewportCmd& v);
    void draw(DrawCmd cmd, const void* vertices, uint32_t vertexCount, const uint16_t* indices, uint32_t indexCount);
    void upload(uint32_t texture, uint32_t level, const Rect& rect, const uint32_t* argb);
    void writeTarget(const Rect& rect, const uint32_t* argb);
    bool empty() const { return commands_.empty(); }
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

private:
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
};

}  // namespace uagfx
