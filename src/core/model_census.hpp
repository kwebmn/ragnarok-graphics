#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// 3.0.20-beta.1 (the owner's «приступай» - stage 1 of «Модели нашим рендером», Claude Doc XMkZ5UMHQXV9CJCvsy82j6): the
// models' colours read from the game's memory and set against its own picture, and the models' share of its frame -
// no change to the picture.
namespace uagfx::modelcensus {

// uaRO.exe's model node (C3dNode; its draw FUN_004DE9F0, thiscall, 3 more on the stack, ret 0xC - a virtual one, its
// first 9 bytes push ebp; mov ebp, esp; sub esp, 0x88): its actor +4 (the actor's alpha byte +0x1ED), its mesh +0x9C
// (the faces' count +4, the faces +0x1C of 0x18 bytes), its faces' colours and texture coordinates +0x118..+0x11C (0x24
// bytes a face: three colours, then u v three times - FUN_004DF930 fills them once from the map's light), its flag
// +0x124 (half see-through this frame, cleared after its draw). The game's ways of colouring them a frame: its
// 0x748878 not 0 (other), the flag (half see-through), 0x7710B9 not 0 (night), else plain
constexpr uint32_t kNodeDraw = 0x4DE9F0;
constexpr size_t kNodeDrawSize = 9;
extern const uint8_t kNodeDrawBytes[kNodeDrawSize];
constexpr uint32_t kNodeActor = 4, kNodeMesh = 0x9C, kMeshFaces = 4, kMeshFaceList = 0x1C, kNodeColours = 0x118,
                   kNodeSeeThrough = 0x124, kActorAlpha = 0x1ED, kOtherWay = 0x748878, kNightWay = 0x7710B9;
constexpr uint32_t kColourSize = 0x24;

enum class Way : uint8_t { Plain, Night, SeeThrough, Other };

struct FaceColours {  // a face's in the game's memory
    uint32_t colour[3] = {};
    float uv[6] = {};
};
struct DrawnVertex {  // the game's drawn vertex (x y z rhw, diffuse +16, specular +20, u v +24)
    uint32_t diffuse = 0;
    float u = 0, v = 0;
};

// A node's drawn vertices (in threes, as its kept faces wrote them, in the faces' order) against its faces in memory:
// each three matched to the next face with the same texture coordinates (the culled ones passed by), up to the first
// three no face further on matches (the block's rest). The colours' three channels each (the alpha left out)
struct Check {
    uint32_t faces = 0, vertices = 0, same = 0;  // matched; their vertices; those the very same
    double ratio = 0;                             // drawn / memory summed over the channels of the vertices apart
    uint32_t ratios = 0;                          // (memory over 8)
    int mostApart = 0;
    void add(const Check& c);
};
Check compare(const std::vector<FaceColours>& faces, const DrawnVertex* drawn, size_t count);

struct Frame {  // a frame's of the game: its model nodes drawn, their faces, the time in its node draws
    uint32_t nodes = 0, faces = 0;
    double ms = 0;
};

class Tally {
public:
    void frame(const Frame& f, double frameMs);
    void check(const Check& c, Way way);
    // "models: the game's own a frame - 1400 nodes, 26000 faces, 2.50 ms (25 % of its frame); colours: 600 vertices of
    // 200 faces checked - 594 (99.0 %) the very same as in its memory, the rest x0.50 on average (12 apart at most);
    // ways: plain 1, night 1, half see-through 0, other 0"; "" before a frame
    std::string line() const;
    void clear() { *this = Tally{}; }

private:
    uint32_t frames_ = 0;
    uint64_t nodes_ = 0, faces_ = 0;
    double ms_ = 0, frameMs_ = 0;
    Check check_;
    uint32_t ways_[4] = {};
};

}  // namespace uagfx::modelcensus
