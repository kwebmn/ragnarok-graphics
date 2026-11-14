// 3.0.19-beta.1 (the testers' doc 14.11: «чон чон мухи не отображаються так як і 2 МВП»; players' logs «reference: no
// sprite for Chonchon (#1011)» and 132 more): the monsters the clients have no sprite for keep the site's picture of
// them in the driver (tools/mob_pictures.py) - «Довідник» and the target card show it in the sprite's place
#include <cstdint>
#include <vector>

#include "core/mob_pictures.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(mob_pictures_the_ones_the_clients_lack) {
    CHECK(mobpictures::count() >= 90);
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    CHECK(mobpictures::pixels(1011, px, w, h));  // Chonchon
    CHECK(w > 16 && h > 16 && px.size() == size_t(w) * size_t(h));
    size_t opaque = 0;
    for (uint32_t c : px) opaque += (c >> 24) == 0xFF;
    CHECK(opaque > px.size() / 10 && opaque < px.size());  // the fly, round it see-through
    CHECK_EQ(px[0] >> 24, 0u);
    for (int id : {1042, 1785, 1873, 1917, 1829, 1830, 1288})  // Steel Chonchon, Atroce, Beelzebub, Satan Morroc...
        CHECK(mobpictures::pixels(id, px, w, h));
    CHECK(!mobpictures::pixels(1002, px, w, h));  // Poring: the client's sprite
    CHECK(!mobpictures::pixels(-1, px, w, h));
}
