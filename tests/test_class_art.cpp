// 3.0.11-beta.178 (the login concept's cards): the classes' pictures (core/class_art) - which job shows which of the
// site's 17 portraits (the transcendent ones', their second classes, babies and mounts with them), their resources in
// ddraw.dll, and a portrait laid over a card's place (its upper part, the rounded corners clear)
#include <cstdint>
#include <string>
#include <vector>

#include "core/class_art.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
std::string art(int job) {
    const char* p = classart::pictureFor(job);
    return p ? p : "";
}
}  // namespace

TEST(class_art_known_jobs) {
    CHECK(art(4009) == "high-priest.jpg" && art(4011) == "whitesmith.jpg" && art(4013) == "assassin-cross.jpg");
    CHECK(art(4008) == "lord-knight.jpg" && art(4014) == "lord-knight.jpg");  // on the Peco Peco too
    CHECK(art(8) == "high-priest.jpg" && art(12) == "assassin-cross.jpg" && art(4031) == "high-priest.jpg");
    CHECK(art(25) == "ninja.jpg" && art(24) == "gunslinger.jpg" && art(4047) == "star-gladiator.jpg");
    CHECK(art(4049) == "soul-linker.jpg" && art(20) == "gypsy.jpg" && art(19) == "clown.jpg");
    CHECK(classart::pictureFor(0) == nullptr && classart::pictureFor(4001) == nullptr);  // the novices, the first
    CHECK(classart::pictureFor(1) == nullptr && classart::pictureFor(99999) == nullptr);
    CHECK(classart::resourceOf("high-priest.jpg") > 0 && classart::resourceOf("nothing.jpg") == 0);
    CHECK(classart::resourceOf("ninja.jpg") != classart::resourceOf("paladin.jpg"));
}

// A 4 x 8 portrait over a 4 x 4 place: its upper half, scaled; the top corners' rounding clear, the bottom's kept
TEST(class_art_cover_keeps_the_upper_part) {
    std::vector<uint32_t> src(4 * 8, 0xFF0000FFu);  // blue
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x) src[size_t(y) * 4 + size_t(x)] = 0xFFFF0000u;  // the upper half red
    panel::Picture out;
    out.width = 8;
    out.height = 8;
    out.px.assign(64, 0u);
    classart::cover(out, panel::Box{0, 0, 8, 8}, src, 4, 8, 3.0f, classart::kTopCorners);
    const uint32_t mid = out.px[4 * 8 + 4];
    CHECK((mid >> 24) == 0xFF && ((mid >> 16) & 0xFF) > 0xC0 && (mid & 0xFF) < 0x40);  // red: the upper part
    CHECK((out.px[0] >> 24) < 0x40);       // the top left corner rounded off
    CHECK((out.px[7 * 8 + 0] >> 24) == 0xFF);  // the bottom left kept square
}

// No portrait for the job: the character's own head in the middle of the place (its picture over what is there)
TEST(class_art_head_in_the_middle) {
    panel::Picture head;
    head.width = 2;
    head.height = 2;
    head.px.assign(4, 0xFFFF0000u);
    panel::Picture out;
    out.width = 6;
    out.height = 6;
    out.px.assign(36, 0xFF000000u);
    classart::centre(out, panel::Box{0, 0, 6, 6}, head);
    CHECK(out.px[2 * 6 + 2] == 0xFFFF0000u && out.px[3 * 6 + 3] == 0xFFFF0000u);
    CHECK(out.px[0] == 0xFF000000u && out.px[5 * 6 + 5] == 0xFF000000u);
}
