#pragma once
#include <cstdint>
#include <vector>

#include "core/d3d7.hpp"

namespace uagfx::batch {

// 0.90: the game's draws gathered into fewer draw calls. The client draws most things as quads of their own - a
// sprite's layer, a piece of a window, a tile: a strip of four vertices each, one DrawPrimitive each - and lists of
// triangles (models, effects' faces). Draws one after another with the same state and their vertices one after
// another in the frame's buffer go to the driver as one: the quads as a list of triangles through one index pattern
// (quadIndices), the lists as they are. Their order and their triangles stay the game's.
enum class Kind : uint8_t {
    Single,  // drawn by itself (a strip of other than four vertices, the game's own indices)
    List,    // a list of triangles
    Quads,   // four-vertex strips
};

struct Draw {
    Kind kind = Kind::Single;
    uint32_t offset = 0;    // its first vertex's byte in the frame's vertices
    uint32_t vertices = 0;
    uint32_t stride = 32;
};

// What a four-vertex strip's index pattern holds: a draw call of indices takes so many quads at most (16-bit).
constexpr uint32_t kMaxQuads = 16384;

// The triangles of quads 0..count-1 as a strip draws them: (0 1 2) (2 1 3), the next four vertices on.
inline std::vector<uint16_t> quadIndices(uint32_t count) {
    std::vector<uint16_t> out;
    out.reserve(size_t(count) * 6);
    for (uint32_t q = 0; q < count; ++q) {
        const uint16_t b = uint16_t(q * 4);
        for (uint16_t i : {uint16_t(0), uint16_t(1), uint16_t(2), uint16_t(2), uint16_t(1), uint16_t(3)})
            out.push_back(uint16_t(b + i));
    }
    return out;
}

// The draw call being gathered: the draws of one kind with their vertices one after another.
class Batch {
public:
    bool open() const { return draws_ > 0; }
    // Whether d goes on it (the state being the same - the caller's to know).
    bool joins(const Draw& d) const {
        if (!open() || d.kind != kind_ || d.kind == Kind::Single || d.stride != stride_ || d.offset != end_)
            return false;
        return d.kind != Kind::Quads || vertices_ / 4 < kMaxQuads;
    }
    void start(const Draw& d) {
        kind_ = d.kind;
        stride_ = d.stride;
        first_ = d.offset;
        end_ = d.offset + d.vertices * d.stride;
        vertices_ = d.vertices;
        draws_ = 1;
    }
    void add(const Draw& d) {
        end_ += d.vertices * d.stride;
        vertices_ += d.vertices;
        ++draws_;
    }
    void close() { draws_ = 0; }
    Kind kind() const { return kind_; }
    uint32_t stride() const { return stride_; }
    uint32_t firstByte() const { return first_; }
    uint32_t vertices() const { return vertices_; }
    uint32_t draws() const { return draws_; }

private:
    Kind kind_ = Kind::Single;
    uint32_t stride_ = 0, first_ = 0, end_ = 0, vertices_ = 0, draws_ = 0;
};

// The kind a draw is of: its primitive (d3d7::PT_*), its vertices, indexed by the game or not.
inline Kind kindOf(uint32_t primitive, uint32_t vertices, bool indexed) {
    if (indexed) return Kind::Single;
    if (primitive == d3d7::PT_TRIANGLELIST && vertices % 3 == 0) return Kind::List;
    if (primitive == d3d7::PT_TRIANGLESTRIP && vertices == 4) return Kind::Quads;
    return Kind::Single;
}

}  // namespace uagfx::batch
