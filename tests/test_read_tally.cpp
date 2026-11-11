#include <string>
#include <vector>

#include "core/read_tally.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

bool has(const std::string& line, const std::string& part) { return line.find(part) != std::string::npos; }

}  // namespace

// 3.0.18-beta.161 (the owner: «делай 1 и 2» - the costs measured before the game's access is redone): the reads of
// the game's memory on its thread - a frame's count and time, and the places that asked the most, as offsets from the
// driver's module (found in the build's link map afterwards)
TEST(read_tally_tells_the_reads_a_frame_and_the_places_that_asked_most) {
    readtally::Tally t;
    const uint32_t base = 0x67000000;
    for (int f = 0; f < 2; ++f) {
        for (int k = 0; k < 10; ++k) t.add(base + 0x1A2B30, 0.01);
        for (int k = 0; k < 4; ++k) t.add(base + 0xFF10, 0.02);
        t.frame();
    }
    const std::string line = t.take(base);
    // 28 reads over 2 frames: 14 a frame, 0.18 ms a frame (0.1 + 0.08)
    CHECK(has(line, "game memory: 14 reads a frame (0.18 ms)"));
    CHECK(has(line, "ddraw+0x1a2b30 10 (0.10 ms)"));
    CHECK(has(line, "ddraw+0x00ff10 4 (0.08 ms)"));
    CHECK(line.find("0x1a2b30") < line.find("0x00ff10"));  // the most first
    CHECK(t.take(base).empty());  // cleared: no frame since
}

TEST(read_tally_keeps_1024_places_apart_and_sums_the_rest_as_elsewhere) {
    // 3.0.18-beta.163 (R-MBCDBU: 64 places kept, «elsewhere 756» of 929 - the reads come from hundreds of places)
    readtally::Tally t;
    for (uint32_t k = 0; k < 1030; ++k) t.add(0x10000 + 16 * k, 0.001);
    t.add(0, 0.001);  // the place not known
    t.frame();
    const std::string line = t.take(0x10000, 3);
    CHECK(has(line, "game memory: 1031 reads a frame"));
    CHECK(has(line, ", elsewhere "));  // 1030 places: some past the 1024 kept
    CHECK(has(line, "unknown 1"));
}

// 3.0.18-beta.163: the places summed by the driver's functions they are in (its unwind table, pe::functionsOf) - the
// line names the functions that read the most, found in the link map by their starts
TEST(read_tally_sums_the_places_by_the_functions_they_are_in) {
    readtally::Tally t;
    const uint32_t base = 0x67000000;
    const std::vector<pe::Function> functions = {{0x1000, 0x100}, {0x2000, 0x80}};
    for (int k = 0; k < 3; ++k) t.add(base + 0x1010, 0.01);  // two calls in the first function
    for (int k = 0; k < 2; ++k) t.add(base + 0x10F0, 0.01);
    for (int k = 0; k < 4; ++k) t.add(base + 0x2004, 0.01);  // the second one
    t.add(base + 0x3000, 0.01);                              // in none: its own place
    t.frame();
    const std::string line = t.take(base, 6, &functions);
    CHECK(has(line, "game memory: 10 reads a frame"));
    CHECK(has(line, "ddraw+0x001000 5 (0.05 ms)"));  // the first function's two places together
    CHECK(has(line, "ddraw+0x002000 4"));
    CHECK(has(line, "ddraw+0x003000 1"));
    CHECK(line.find("0x001000") < line.find("0x002000"));
}

TEST(read_tally_says_nothing_without_a_frame_and_places_outside_the_module_as_they_are) {
    readtally::Tally t;
    t.add(0x500000, 0.5);
    CHECK(t.take(0x67000000).empty());  // no frame shown: nothing to say
    t.add(0x500000, 0.5);
    t.frame();
    const std::string line = t.take(0x67000000);
    CHECK(has(line, "game memory: 1 reads a frame (0.50 ms)"));
    CHECK(has(line, "0x00500000 1"));  // before the module: its own address
}
