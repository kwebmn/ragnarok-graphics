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
