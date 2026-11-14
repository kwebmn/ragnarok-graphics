#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

namespace uagfx::mobpictures {

// 3.0.19-beta.1 (the testers' doc 14.11: «чон чон мухи не отображаються»; players' logs «reference: no sprite for
// Chonchon (#1011)» and 132 more): the pictures of the monsters the clients have no sprite for - the site's
// (web/assets/mobs/<id>.gif's first frame), kept in the driver by tools/mob_pictures.py
// (src/core/mob_pictures.inc): «Довідник» and the target card show one where the monster's sprite is not found.
// `argb` w x h, see-through round it (alpha 0); false - none kept for that monster
bool pixels(int id, std::vector<uint32_t>& argb, int& w, int& h);
size_t count();  // the monsters kept

}  // namespace uagfx::mobpictures
