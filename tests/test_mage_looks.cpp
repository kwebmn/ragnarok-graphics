// 3.0.15-beta.11: [effects] mage_effects - the mages' skills by element, their parts by how the game draws them
#include "core/mage_looks.hpp"
#include "test.hpp"

using namespace uagfx;
using magelook::Element;
using magelook::Part;
using magelook::Branch;

TEST(mage_looks_parts_and_ids) {
    CHECK(magelook::partOf(true, 5, 2, 4, 100, 1600) == Part::Layer);  // added on
    CHECK(magelook::partOf(true, 5, 2, 4, 800, 1600) == Part::Haze);   // a wide one
    CHECK(magelook::partOf(true, 5, 6, 3, 50, 1600) == Part::Solid);   // a solid's face
    CHECK(magelook::partOf(true, 5, 6, 4, 50, 1600) == Part::None);    // a laid-over sprite: as the game draws it
    CHECK(magelook::partOf(false, 5, 2, 4, 50, 1600) == Part::None);
    const uint16_t id = magelook::idOf(Part::Solid, Element::Earth);
    CHECK(magelook::isOurs(id) && magelook::partOfId(id) == Part::Solid && magelook::elementOfId(id) == Element::Earth);
    CHECK(!magelook::isOurs(89) && !magelook::isOurs(0xFFF1));
    CHECK(magelook::paletteOf(Element::Fire).edge[0] > magelook::paletteOf(Element::Fire).edge[2]);  // orange
}

// 3.0.16-beta.10 (the owner: «эффект крутящийся вокруг героя ... не серьезно выглядит»; R-45TDN7): the spirit
// spheres' added passes their own orb, still added on; the rest of the effects as before
TEST(mage_looks_spheres_are_orbs) {
    using magelook::Part;
    CHECK(magelook::partFor(228, Part::Layer) == Part::Orb);
    CHECK(magelook::partFor(228, Part::Haze) == Part::Orb);
    CHECK(magelook::partFor(228, Part::None) == Part::None);
    CHECK(magelook::partFor(16, Part::Layer) == Part::Layer);
    CHECK(magelook::partOfId(magelook::idOf(Part::Orb, magelook::Element::Spirit)) == Part::Orb);
}

// 3.0.16-beta.27: [effects] mage_soft - a layer and a haze soft, the spheres, Fire Wall and the solids as they are
TEST(mage_looks_soft_style) {
    CHECK(magelook::styled(magelook::Part::Layer, true) == magelook::Part::Soft);
    CHECK(magelook::styled(magelook::Part::Haze, true) == magelook::Part::Soft);
    CHECK(magelook::styled(magelook::Part::Layer, false) == magelook::Part::Layer);
    CHECK(magelook::styled(magelook::Part::Orb, true) == magelook::Part::Orb);
    CHECK(magelook::styled(magelook::Part::Flame, true) == magelook::Part::Flame);
    CHECK(magelook::styled(magelook::Part::Solid, true) == magelook::Part::Solid);
}
