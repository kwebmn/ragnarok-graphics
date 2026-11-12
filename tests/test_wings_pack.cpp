// 3.0.18-beta.230 (the wings, spec docs/superpowers/specs/2026-11-13-wings-design.md): dist/wings.grf - the uaRO HD
// client's garment sets (tools/wings_pack.py) as the driver reads them (uaro-graphics\wings.grf, core grf::Archive)
#include <string>
#include <vector>

#include "core/grf/grf.hpp"
#include "core/sprite_file.hpp"
#include "core/wings.hpp"
#include "test.hpp"

using namespace uagfx;
using wings::Set;

namespace {

// CP949: data\sprite\인간족\몸통\<sex>\<body>.spr - a novice boy's and an acolyte girl's bodies
const std::string kBodies = "data\\sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\";
const std::string kNoviceBoy = kBodies + "\xB3\xB2\\\xC3\xCA\xBA\xB8\xC0\xDA_\xB3\xB2.spr";
const std::string kAcolyteGirl = kBodies + "\xBF\xA9\\\xBC\xBA\xC1\xF7\xC0\xDA_\xBF\xA9.spr";

std::string read(grf::Archive& a, const std::string& name) {
    std::vector<uint8_t> bytes;
    std::string error;
    if (!a.read(name, bytes, error)) return {};
    return std::string(bytes.begin(), bytes.end());
}

}  // namespace

TEST(wings_pack_reads_the_sets) {
    grf::Archive a;
    std::string error;
    CHECK(a.openToRead(std::string(UAGFX_SOURCE_DIR) + "/dist/wings.grf", error));
    spritefile::Act act;
    CHECK(spritefile::readAct(read(a, wings::garmentEntry(Set::Angel, kNoviceBoy, "act")), act));
    CHECK_EQ(int(act.actions.size()), 104);  // 13 actions of 8 sides, as a body's
    CHECK(!act.actions[0].empty() && !act.actions[0][0].anchors.empty());
    spritefile::Sprite spr;
    CHECK(spritefile::readSpr(read(a, wings::garmentEntry(Set::Fallen, kAcolyteGirl, "spr")), spr));
    CHECK(!spr.palette.empty());
    spritefile::Act dark;
    CHECK(spritefile::readAct(read(a, wings::garmentEntry(Set::Dark, kNoviceBoy, "act")), dark));
    CHECK(read(a, "angel\\nobody.act").empty());  // a body the sets lack
}

TEST(wings_pack_missing_file) {
    grf::Archive a;
    std::string error;
    CHECK(!a.openToRead(std::string(UAGFX_SOURCE_DIR) + "/dist/no-such-wings.grf", error));
    CHECK(!error.empty());
}
