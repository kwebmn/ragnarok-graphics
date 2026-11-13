#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/sprite_file.hpp"

namespace uagfx::wings {

// 3.0.18-beta.230 (the owner: wings on the characters - concept https://claude.ai/artifact/S1rYPnLYM9yByk6wz5FTok,
// spec docs/superpowers/specs/2026-11-13-wings-design.md): big wings on whoever wears one of nine GM-only headgears.
// The server tells every client the headgears' views; nine of them carry a look, drawn by the driver from the uaRO HD
// client's garment sets (Angel Wings, Wings of Fallen Angel, Dark Wing) or their recolours.
enum class Look : uint8_t { None, Angel, Fallen, Demon, Fire, Ice, Valkyrie, Gold, Sakura, Crimson };
Look lookOfView(int view);           // .232: 260 angel, 233 fallen, 129 demon, 221 fire, 301 ice, 300 valkyrie,
                                     // 261 gold, 325 sakura, 271 crimson; else none
Look lookOfViews(const int views[3]);  // the first of a character's three headgears with a look
const char* lookName(Look look);       // "angel" ... (the log's)

enum class Set : uint8_t { Angel, Fallen, Dark };
Set setOf(Look look);  // the garment set a look draws (a recolour: its source's)
// A look's colours over its set's palette pictures: their distinct colours by brightness onto the look's gradient
// (none for the sets' own looks)
void recolour(spritefile::Sprite& s, Look look);
// The pack's name of a set's file for a body: "angel\<sex>\<body>.<ext>", "fallen\...", "dark.<ext>" (one for every
// body); "" when `bodySprite` is not a human body's sprite (its folder 인간족\몸통\<sex>\<body>.spr, CP949, any case;
// .235: whatever comes before it - the game's resources keep "sprite\..." without data\)
std::string garmentEntry(Set set, const std::string& bodySprite, const char* ext);
// .235: a sprite resource's name as the game's files know it: "data\sprite\..." ("sprite\..." gets data\, a name
// relative to the sprites' folder both)
std::string dataPath(const std::string& resource);
// .236 (R-4U3RD7): a body's act by its sprite's name (the extension changed); "" when the name has none
std::string actOf(const std::string& sprite);
// 3.0.18-beta.231: a headgear view past the game's own table of their names (a vector of name pointers, its first and
// last at uaRO.exe's session +0x880/+0x884) - the game would read another table's word for it and ask for a sprite
// that is not there; none (0) is the game's own
bool pastTable(int view, uint32_t first, uint32_t last);

// Where a character's garment frame is drawn: its screen point (the game's, of its feet) and its scale (its size times
// the depth's, as uaRO.exe FUN_005ae540 has them)
struct Transform {
    float x = 0, y = 0, scale = 1;
};
struct Quad {
    float x0 = 0, y0 = 0, x1 = 0, y1 = 0;  // screen pixels
    int image = -1, type = 0;              // the sprite's picture (type 0 palette, 1 true colour)
    bool mirror = false;
    uint32_t tint = 0xFFFFFFFF;
};
// The garment frame at the body's action and frame (each `frame % frames` of its own action) on the body: by the body
// frame's anchor less the garment frame's (either missing: 0), a layer's picture round its centre times its scale and
// the character's; layers with no picture and an action the garment lacks: none
std::vector<Quad> quads(const spritefile::Act& garment, const spritefile::Act& body, const spritefile::Sprite& s,
                        size_t action, size_t frame, const Transform& t);
bool facesAway(size_t action);  // its side (of 8) 3, 4 or 5: the wings over the body, else behind it
// A face's depth (z, rhw) at another height of the screen: on the line through its top's and bottom's
float lerpAt(float y, float y0, float v0, float y1, float v1);

// 3.0.18-beta.237 (R-FAFJWS «мигает тело персонажа»): a face's depth along the screen's height - its rhw at its top
// and at its bottom (the rasterizer's rhw is linear in screen space)
struct Depth {
    float yTop = 0, rhwTop = 0, yBottom = 0, rhwBottom = 0;
    bool known() const { return rhwTop > 0.0f && rhwBottom > 0.0f; }
    float at(float y) const { return lerpAt(y, yTop, rhwTop, yBottom, rhwBottom); }
};
// Of TL1 vertices (32 bytes each: x, y, z, rhw, ...): their top's and bottom's; none known for no vertices
Depth faceDepth(const uint8_t* vertices, uint32_t count);
float zOf(float rhw);                       // uaRO.exe's depth of an rhw (FUN_00412E30): far/(far-near)*(1-near*rhw)
Depth moved(const Depth& d, float units);  // `units` farther in view space (less than 0: nearer)

// A frame's bookkeeping (the driver's draws, win/wings): each wearer's faces as the game sorts them; its wings right
// before its first face drawn (facing us) or right after its last (facing away). .237: their depth a little behind
// the farthest of its faces (facing us) or nearer than the nearest (facing away) - uaRO.exe sorts a body and its head
// by their tops' rhw, the head now and then first: the first face's depth (the head's) hid the body under the wings
class Frame {
public:
    // a wearer's job (its index); .237: feetY, the screen row its faces' depths are compared at
    int begin(uint32_t actor, bool away, float feetY = 0.0f);
    // one of its faces (job -1: nobody's), its depth (none known: the draw's own face's then); .237: anchor false - a
    // face of the game's opaque list (drawn before the ground) counts for the depth alone, no wings go to its draw
    void face(int job, uint32_t vertices, const Depth& depth = {}, bool anchor = true);
    struct Emit {
        int before = -1, after = -1;  // the job whose wings go before / after this draw
        Depth depth;                  // .237: their depth (none known: the draw's own face's)
    };
    Emit drawn(uint32_t vertices);
    void clear();
    uint32_t actor(int job) const;

private:
    struct Job {
        uint32_t actor = 0;
        bool away = false;
        int faces = 0, drawn = 0;
        float feetY = 0;
        Depth farthest, nearest;  // .237: of all its faces at feetY
    };
    std::vector<Job> jobs_;
    std::unordered_map<uint32_t, int> byVertices_;
};

}  // namespace uagfx::wings
