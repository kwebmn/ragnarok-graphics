// 3.0.15-beta.11: [effects] mage_effects - the mages' skills by element, their parts by how the game draws them
#include "core/mage_looks.hpp"
#include "test.hpp"

using namespace uagfx;
using magelook::Element;
using magelook::Part;

TEST(mage_looks_elements_by_effect) {
    CHECK(magelook::elementOf(31) == Element::Fire);    // Fire Bolt
    CHECK(magelook::elementOf(92) == Element::Fire);    // Meteor Storm
    CHECK(magelook::elementOf(89) == Element::Water);   // Storm Gust
    CHECK(magelook::elementOf(26) == Element::Water);   // Cold Bolt
    CHECK(magelook::elementOf(90) == Element::Wind);    // Lord of Vermilion
    CHECK(magelook::elementOf(93) == Element::Wind);    // Jupitel Thunder
    CHECK(magelook::elementOf(142) == Element::Earth);  // Heaven's Drive
    CHECK(magelook::elementOf(15) == Element::Spirit);  // Soul Strike
    CHECK(magelook::elementOf(12) == Element::None);    // the casting has its own look
    CHECK(magelook::elementOf(312) == Element::None);   // Heal: not a mage's
}

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
