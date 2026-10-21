#pragma once
#include <cstdint>
#include <functional>
#include <vector>

#include "core/game_camera.hpp"
#include "core/living.hpp"
#include "core/torch.hpp"

namespace uagfx::critters {

// 1.7, the living world's creatures and particles (win/gpu/critters.cpp draws them with critters_ps): butterflies
// and birds by day, leaves falling from the crowns, embers rising from the fires, breath in the cold, drops where a
// step goes into the water, drips in the caves, bubbles under the sea. They live in the game's world (up along -y; a
// character's mouth about 15 units over its feet - 2.7.44, not the 7 units tall once written here) about the player;
// the renderer turns them to the screen each frame.
enum Kind : uint8_t {
    Butterfly = 1,
    Dragonfly = 2,
    BirdGround = 3,
    BirdFlying = 4,
    Leaf = 5,
    Ember = 6,
    Puff = 7,
    Droplet = 8,
    Bubble = 9,
    Drip = 10,
    Ring = 11,    // on the ground, flat
    Shadow = 12,  // on the ground, flat
    // 13: the light from above under the sea - the renderer's own, over the whole picture
    Firefly = 14,  // 2.6.11: a light over the grass at night, blinking (p1 how lit, 0-1)
};

struct Point {
    float x = 0, y = 0, z = 0;
};

// One to draw: where (the world), how large (world units), its kind's numbers (critters_ps): a phase, a colour or a
// heading - `heading` a direction in the world along the ground, turned to the screen by the renderer.
struct Sprite {
    Point at;
    Kind kind = Butterfly;
    float size = 1.0f;
    float p1 = 0.0f, p2 = 0.0f;
    float alpha = 1.0f;
    float headingX = 0.0f, headingZ = 0.0f;  // 0, 0: none
};

struct Switches {
    bool butterflies = false;
    bool dragonflies = false;  // 2.4: over the water, with the butterflies (a box of their own under them)
    bool birds = false;
    bool leaves = false;
    bool embers = false;
    bool breath = false;
    bool splashes = false;
    bool drips = false;
    bool underwater = false;
    bool nightFireflies = false;  // 2.6.11: over the grass from dusk to dawn
    bool any() const {
        return butterflies || birds || leaves || embers || breath || splashes || drips || underwater ||
               nightFireflies;
    }
};

struct Crown {  // a tree's leaves drawn this frame
    Point centre;
    float radius = 3.0f;
};

struct Frame {
    double now = 0.0;   // seconds
    float dt = 0.0f;    // since the last frame, at most 0.1
    Point centre;       // the player (the point the camera looks at); nothing lives without it
    bool centreKnown = false;
    std::vector<Point> feet;    // the characters' feet this frame
    std::vector<Point> steps;   // the steps that began this frame
    std::vector<Point> fires;   // the flames' tips (fireOf)
    std::vector<Crown> crowns;  // the trees in sight
    float waterLevel = 0.0f;    // the water's height, when known
    bool waterKnown = false;
    // 2.7.34: the map's ground at a point - its height (world y) into y, false not known (no .gnd read, the axes not
    // found): the birds settle on it only over the water (the tester in Alberta: «уточек с воды убери»)
    std::function<bool(const Point&, float&)> ground;
    float day = 1.0f;     // 0-1: daylight (living::daytime)
    float night = 0.0f;   // 0-1
};

// 2.6.12: where a flame the game drew burns in the world - its tip, four fifths of the way up from its base, back
// from the screen by the game's projection and the camera backwards (view -> world, 4 x 3 rows): the embers rise
// from it. false for a flame with no depth.
bool fireOf(const torch::Flame& f, const camera::Projection& p, const float viewToWorld[12], Point& out);

class World {
public:
    void setPlace(const living::Place& p);  // a new map: everything goes
    void update(const Switches& on, const Frame& f);
    const std::vector<Sprite>& sprites() const { return out_; }

private:
    struct Fly {
        Point at;
        float altitude = 3.0f, heading = 0.0f, phase = 0.0f, colour = 0.0f;
        bool dragonfly = false;
        float hover = 0.0f;  // a dragonfly's wait before its next dart
        Point target;
    };
    struct Bird {
        Point at;
        Point away;  // its flight, units a second
        float phase = 0.0f, flown = 0.0f;  // flown: seconds since it was startled (below 0: about to go)
        float facing = 1.0f;
        bool scared = false;
    };
    struct Mote {  // what falls, rises or flies off: leaves, embers, puffs, drops, bubbles
        Kind kind = Leaf;
        Point at, v;
        float age = 0.0f, life = 1.0f, size = 0.3f, spin = 0.0f, colour = 0.0f, ground = 0.0f;
        bool landed = false;
    };
    struct DripSpot {
        Point at;
        float period = 3.0f, offset = 0.0f;
    };
    struct Vent {
        Point at;
        float next = 0.0f;
    };
    struct Glow {  // 2.6.11: a firefly in the grass at night
        Point at;
        float heading = 0.0f, phase = 0.0f, period = 3.0f, altitude = 1.0f;
    };

    float random();  // 0-1
    Point around(const Point& c, float near, float far);
    void flies(const Switches& on, const Frame& f);
    void birds(const Switches& on, const Frame& f);
    void motes(const Switches& on, const Frame& f);
    void drips(const Switches& on, const Frame& f);
    void fireflies(const Switches& on, const Frame& f);  // 2.6.11

    living::Place place_;
    uint32_t seed_ = 0x9e3779b9u;
    std::vector<Fly> flies_;
    std::vector<Bird> flock_;
    double flockAgain_ = 0.0;  // when the next flock may come
    std::vector<Mote> motes_;
    std::vector<DripSpot> drips_;
    std::vector<Vent> vents_;
    std::vector<Glow> glows_;
    float fireCarry_ = 0.0f, leafCarry_ = 0.0f;
    std::vector<Sprite> out_;
};

constexpr size_t kMaxSprites = 192;

}  // namespace uagfx::critters
