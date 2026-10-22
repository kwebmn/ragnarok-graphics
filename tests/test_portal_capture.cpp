#include <string>
#include <vector>

#include "core/portal_capture.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(portal_capture_two_seconds_from_the_first_portal_on_a_map) {
    // 2.7.53 (the tester after five builds of the new portal: «посмотри с самого начала ... сделаем порталы обдуманно
    // и правильно»): the facts first - the game's portal draws recorded for 120 frames from the first one on a map
    portal_capture::Recorder r;
    CHECK(r.take("prontera"));
    for (int f = 0; f < 119; ++f) r.endFrame();
    CHECK(r.take("prontera"));  // the 120th frame
    r.endFrame();
    CHECK(!r.take("prontera"));  // done on this map
    CHECK(r.take("payon"));      // another map: two seconds more
}

TEST(portal_capture_three_maps_at_most) {
    portal_capture::Recorder r;
    for (const char* map : {"a", "b", "c"}) {
        CHECK(r.take(map));
        for (int f = 0; f < 120; ++f) r.endFrame();
    }
    CHECK(!r.take("d"));
}

TEST(portal_capture_a_draw_its_states_and_vertices) {
    DrawSetup s;
    s.blend = true;
    s.srcBlend = d3d7::BLEND_SRCALPHA;
    s.dstBlend = d3d7::BLEND_ONE;
    s.zEnable = true;
    const portal_capture::Vertex v[3] = {{565.0f, 559.0f, 0.99f, 0.0021f, 0x80FFFFFFu, 0u, 0.5f, 0.5f},
                                         {613.0f, 466.0f, 0.99f, 0.0021f, 0x80FFFFFFu, 0u, 1.0f, 0.0f},
                                         {533.0f, 466.0f, 0.99f, 0.0021f, 0x00FFFFFFu, 0u, 0.0f, 0.0f}};
    const std::vector<std::string> lines = portal_capture::drawLines(7, 321, s, 12345u, 64, 64, v, 3, nullptr, 0);
    CHECK(lines.size() == 4);
    CHECK(lines[0].find("frame 7") != std::string::npos);
    CHECK(lines[0].find("effect 321") != std::string::npos);
    CHECK(lines[0].find("blend 5/2") != std::string::npos);  // SRCALPHA, ONE
    CHECK(lines[0].find("texture 12345 64x64") != std::string::npos);
    CHECK(lines[1].find("565.0 559.0") != std::string::npos);
    CHECK(lines[1].find("80FFFFFF") != std::string::npos);
    CHECK(lines[3].find("0.000 0.000") != std::string::npos);
}

TEST(portal_capture_a_texture_in_base64) {
    const uint32_t texels[2] = {0xFF000000u, 0x00FFFFFFu};
    const std::vector<std::string> lines = portal_capture::textureLines(9, 2, 1, texels);
    CHECK(!lines.empty());
    CHECK(lines[0].find("texture 9 2x1") != std::string::npos);
    CHECK(lines.size() >= 2 && lines[1].find("AAAA/////wA=") != std::string::npos);  // little-endian ARGB
}
