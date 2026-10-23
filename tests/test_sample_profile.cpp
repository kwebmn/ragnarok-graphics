#include "core/sample_profile.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(profile_tallies_modules_places_and_callers) {
    // 0.87: by module, by 64-byte place in uaRO.exe, and the uaRO.exe place that called another module.
    profile::Tally t;
    t.setModules({{"uaRO.exe", 0x400000, 0x300000}, {"ntdll.dll", 0x77000000, 0x100000}}, 0);
    for (int k = 0; k < 6; ++k) t.add(0x412cb4 + uint32_t(k), 0);  // one place
    for (int k = 0; k < 3; ++k) t.add(0x40e0d0, 0);
    t.add(0x77001234, 0x4170a5);
    t.add(0x12345678, 0);
    CHECK(t.samples() == 11);
    const std::vector<std::string> lines = t.report(10.0);
    CHECK(lines.size() == 3);
    CHECK(lines[0].find("uaRO.exe 81.8%") != std::string::npos);
    CHECK(lines[0].find("ntdll.dll 9.1%") != std::string::npos && lines[0].find("(no module)") != std::string::npos);
    CHECK(lines[1].find("0x412c80 54.5%, 0x40e0c0 27.3%") != std::string::npos);
    CHECK(lines[2].find("0x4170a0 into ntdll.dll 9.1%") != std::string::npos);
    CHECK(t.samples() == 0 && t.report(1.0).empty());
}

TEST(profile_tallies_our_own_places_by_offset) {
    // 2.5.5: the driver's own module (ddraw.dll) by 64-byte place too, as offsets from its base - its functions
    // found afterwards in the build's link map (it may load anywhere)
    profile::Tally t;
    t.setModules({{"uaRO.exe", 0x400000, 0x300000}, {"DDRAW.dll", 0x6a000000, 0x400000}}, 0, 1);
    for (int k = 0; k < 4; ++k) t.add(0x6a012345, 0x4170a5);
    t.add(0x6a3000f0, 0);
    const std::vector<std::string> lines = t.report(10.0);
    bool found = false;
    for (const std::string& l : lines)
        if (l.find("DDRAW.dll's busiest places - +0x012340 80.0%, +0x3000c0 20.0%") != std::string::npos) found = true;
    CHECK(found);
}

TEST(profile_tallies_windows_functions_and_the_games_steps) {
    // 3.0.2-beta.12 (a map's loading measured): Windows' code by the function the game or a library called ("Sleep"),
    // the game's step a sample is under, and the lines under a prefix of their own
    profile::Tally t;
    t.setModules({{"uaRO.exe", 0x400000, 0x300000},
                  {"ntdll.dll", 0x77000000, 0x100000},
                  {"cps.dll", 0x10000000, 0x10000}},
                 0);
    for (int k = 0; k < 5; ++k) t.add({0x77001234, 0x559800, "Sleep", int(profile::Step::Frames)});
    for (int k = 0; k < 3; ++k) t.add({0x77001300, 0x510ff5, "ReadFile", int(profile::Step::Files)});
    t.add({0x10001000, 0x511090, "", int(profile::Step::Files)});
    t.add({0x4eab00, 0, "", int(profile::Step::World)});
    const std::vector<std::string> lines = t.report("map prontera", "loaded in 3.69 s");
    CHECK(lines.size() == 4);
    CHECK_EQ(lines[0], std::string("map prontera: loaded in 3.69 s - Sleep 50.0%, ReadFile 30.0%, cps.dll 10.0%, "
                                   "uaRO.exe 10.0%"));
    CHECK_EQ(lines[1], std::string("map prontera: by the game's steps - the game's frames 50.0%, reading the game's "
                                   "files 40.0%, the world and its near models 10.0%"));
    CHECK(lines[2].find("map prontera: uaRO.exe's busiest places - 0x4eab00 10.0%") == 0);
    CHECK(lines[3].find("0x559800 into Sleep 50.0%, 0x510ff0 into ReadFile 30.0%, 0x511090 into cps.dll 10.0%") !=
          std::string::npos);
}

TEST(profile_steps_not_known_said_as_elsewhere) {
    // a sample under none of the steps: "elsewhere", once there are steps at all
    profile::Tally t;
    t.setModules({{"uaRO.exe", 0x400000, 0x300000}}, 0);
    t.add({0x4eab00, 0, "", int(profile::Step::World)});
    t.add({0x401000, 0, "", -1});
    const std::vector<std::string> lines = t.report("map prontera", "loaded in 1.00 s");
    CHECK_EQ(lines[1], std::string("map prontera: by the game's steps - the world and its near models 50.0%, "
                                   "elsewhere 50.0%"));
}

TEST(profile_step_the_innermost_known) {
    // the sample's own place first, then its return addresses outward: the innermost of uaRO.exe's known functions
    using profile::Step;
    const uint32_t underTheMap[] = {0x510ff5, 0x50d1a0, 0x5132a0, 0x4eac00, 0x559100};
    CHECK_EQ(profile::stepOf(0x77001234, underTheMap, 5), int(Step::Files));  // Windows' ReadFile in the GRF read
    const uint32_t parse[] = {0x5132a0, 0x4eac00, 0x559100};
    CHECK_EQ(profile::stepOf(0x45a000, parse, 3), int(Step::Resources));  // a model parsed by the resources
    const uint32_t world[] = {0x559100};
    CHECK_EQ(profile::stepOf(0x4eab00, world, 1), int(Step::World));  // the map's load itself
    const uint32_t frame[] = {0x5597a0};
    CHECK_EQ(profile::stepOf(0x579a00, frame, 1), int(Step::Packets));  // its own place first
    CHECK_EQ(profile::stepOf(0x401000, nullptr, 0), -1);
    // 3.0.2-beta.13: the ray's test with a triangle under the cell test, under the game's pick (or ours, in ddraw.dll)
    const uint32_t pick[] = {0x42aa9c, 0x564ad5, 0x6a012345, 0x566514, 0x5597a0};
    CHECK_EQ(profile::stepOf(0x40b2c0, pick, 5), int(Step::Pick));
    CHECK_EQ(std::string(profile::stepName(int(Step::Pick))), std::string("the cell under the mouse"));
    CHECK_EQ(std::string(profile::stepName(int(Step::Present))), std::string("showing a frame"));
    CHECK_EQ(std::string(profile::stepName(-1)), std::string("elsewhere"));
}
