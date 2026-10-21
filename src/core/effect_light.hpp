#pragma once
#include <cstdint>
#include <vector>

namespace uagfx::effect_light {

// 0.26: how much a picture of an effect looks like fire, 0-1 - its pixels weighed by their alpha and brightness (an
// additive picture's black adds nothing), their average red well above blue, green between them; colour: that
// average, its brightest channel 1. The fire of skills lights around it (win/gpu/torches.cpp).
float fireOf(const uint32_t* argb, int width, int height, float colour[3]);
// 0.36: how much a picture of any other effect lights around it in its own colour, 0-1 - saturated colours (ice blue,
// poison green, holy gold) by how saturated they are, near-white ones only when bright (lightning); grey, dark and
// clear pictures none. colour as fireOf's.
float lightOf(const uint32_t* argb, int width, int height, float colour[3]);
// 2.7.20: how much a place of skills' light lights (0-1) from its weight w - the sum of its draws' strengths - with
// night (0-1) as much night as it is: at night at least 0.7 (the tester: «рувотч не светит» - its blue mist a place
// of about 0.2, a tenth of a torch's light), the faintest places (under 0.05) in proportion; by day w as it is.
float skillWeight(float w, float night);
// 2.7.21: whether the game's effect `id` (effect_exe::Tag::id) is an orb of light the game lays over the world as a
// sprite - not added, so neither fireOf nor lightOf ever sees it (the tester: «рувотч не светит»): Ruwach's (33,
// 이팩트\particle2.spr round the caster, uaRO.exe 0x5CC5D0) pale blue, Sight's (22, its fireball, 0x5CA6F0) fire;
// colour its light's, its brightest channel 1.
bool orbOf(int id, float colour[3]);
// 2.7.22 (the user on 2.7.21's Ruwach: «мне кажется слишком ярко ... чтобы было спокойнее»): how an orb of light's
// place lights - w its draws' weight, night 0-1, fire «Fire of skills»'s strength (0-2): a little weaker than
// 2.7.21's (0.5 at night, half by day), `height` world units over its draws' middle - over the caster's head, no hot
// spot on the ground where the orb flies - reaching `reach`.
struct OrbLight {
    float strength, reach, height;
};
OrbLight orbLight(float w, float night, float fire);
// 2.7.22: the orbs' draws (a Spark's orb set: Ruwach's orbs and their shadows) gathered by caster - each into the
// place whose middle so far is within `radius` game pixels, else a place of its own (2.7.21's cells of 80 pixels
// split an orbit into two lights that jumped as it flew); a place's sums weighed by its draws' strengths.
struct OrbPlace {
    float w = 0, view[3] = {0, 0, 0}, colour[3] = {0, 0, 0}, gameX = 0, gameY = 0;
};
template <typename Spark>
std::vector<OrbPlace> gatherOrbs(const std::vector<Spark>& sparks, float radius) {
    std::vector<OrbPlace> places;
    for (const Spark& s : sparks) {
        if (!s.orb || s.strength <= 0.0f) continue;
        OrbPlace* at = nullptr;
        for (OrbPlace& p : places) {
            const float dx = p.gameX / p.w - s.gameX, dy = p.gameY / p.w - s.gameY;
            if (dx * dx + dy * dy <= radius * radius) {
                at = &p;
                break;
            }
        }
        if (!at) at = &places.emplace_back();
        at->w += s.strength;
        at->gameX += s.gameX * s.strength;
        at->gameY += s.gameY * s.strength;
        for (int k = 0; k < 3; ++k) {
            at->view[k] += s.view[k] * s.strength;
            at->colour[k] += s.colour[k] * s.strength;
        }
    }
    return places;
}

}  // namespace uagfx::effect_light
