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
// +0x124 (half see-through this frame, cleared after its draw), its vertices' ints +0x144..+0x148 (9 a face: each
// vertex's colour plus the light of its place - FUN_004E0070). The game's ways of colouring them a frame (its code
// 0x4DEDB0-0x4DEF7F): 0x748878 not 0 - other (the colour bytes x 0x7710D8/DC/E0); the flag - half see-through (the
// colour as it is, alpha 0x80); 0x7710B9 not 0 - lit (the vertex's ints x 0x7710E4/E8/EC); else plain (the colour
// bytes x the same); each product truncated, its low byte stored; alpha the actor's byte
constexpr uint32_t kNodeDraw = 0x4DE9F0;
constexpr size_t kNodeDrawSize = 9;
extern const uint8_t kNodeDrawBytes[kNodeDrawSize];
constexpr uint32_t kNodeActor = 4, kNodeMesh = 0x9C, kMeshFaces = 4, kMeshFaceList = 0x1C, kNodeColours = 0x118,
                   kNodeSeeThrough = 0x124, kNodeLit = 0x144, kActorAlpha = 0x1ED, kOtherWay = 0x748878,
                   kLitWay = 0x7710B9, kFactors = 0x7710E4, kOtherFactors = 0x7710D8;
constexpr uint32_t kColourSize = 0x24;

enum class Way : uint8_t { Plain, Lit, SeeThrough, Other };

struct FaceColours {  // a face's in the game's memory
    uint32_t colour[3] = {};
    float uv[6] = {};
};
struct FaceLit {  // a face's three vertices' ints at +0x144 (the colour plus its place's light): r g b each
    int32_t rgb[9] = {};
};
struct DrawnVertex {  // the game's drawn vertex (x y z rhw, diffuse +16, specular +20, u v +24)
    uint32_t diffuse = 0;
    float u = 0, v = 0;
};
// The game's colouring of a node this frame: its way, the three factors (for bytes 2 1 0), the actor's alpha
struct Colouring {
    Way way = Way::Plain;
    float factors[3] = {1.0f, 1.0f, 1.0f};
    uint8_t alpha = 0xFF;
};
// A vertex's colour as the game makes it from its memory (`lit`: that vertex's three ints, lit only)
uint32_t expected(const Colouring& c, uint32_t memory, const int32_t lit[3]);

// A node's drawn vertices (in threes, as its kept faces wrote them, in the faces' order) against its faces in memory:
// each three matched to the next face with the same texture coordinates (the culled ones passed by), up to the first
// three no face further on matches (the block's rest); each vertex against expected() - the very same 32 bits.
// `lits` the faces' +0x144 (lit needs them, one a face; else nothing checked)
struct Check {
    uint32_t faces = 0, vertices = 0, same = 0;  // matched; their vertices; those the very same
    int mostApart = 0;                            // a channel's (alpha too) of the rest
    void add(const Check& c);
};
Check compare(const std::vector<FaceColours>& faces, const std::vector<FaceLit>& lits, const Colouring& c,
              const DrawnVertex* drawn, size_t count);

struct Frame {  // a frame's of the game: its model nodes drawn, their faces, the time in its node draws
    uint32_t nodes = 0, faces = 0;
    double ms = 0;
};

class Tally {
public:
    void frame(const Frame& f, double frameMs);
    void check(const Check& c, Way way);
    // "models: the game's own a frame - 1400 nodes, 26000 faces, 2.50 ms (25 % of its frame); colours: 600 vertices of
    // 200 faces checked - 594 (99.0 %) as the game colours them from its memory (12 apart at most); ways: plain 1,
    // lit 1, half see-through 0, other 0"; "" before a frame
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
