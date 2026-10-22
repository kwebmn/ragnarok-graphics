#include <string>

#include "core/settings/auto_setup.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

std::string valueOf(const AutoSetup& a, const std::string& section, const std::string& key) {
    for (const IniEdit& e : a.edits)
        if (e.section == section && e.key == key) return e.value;
    return "";
}

Machine rtx5060() {  // the developer's: Windows 11, RTX 5060 Ti 16 GB, 4K
    Machine m;
    m.adapter = "NVIDIA GeForce RTX 5060 Ti";
    m.windows10 = m.d3d12 = m.d3d11 = true;
    m.d3d11Level = 0xb100;
    m.vendor = 0x10de;
    m.vramMb = 16384;
    m.screenWidth = 3840;
    m.screenHeight = 2160;
    m.pack = true;
    return m;
}

}  // namespace

TEST(auto_setup_a_strong_nvidia_card) {
    const AutoSetup a = autoSetup(rtx5060());
    CHECK(a.tier == Tier::High && a.preset == Preset::Balance);
    CHECK(valueOf(a, "renderer", "api") == "auto");
    CHECK(valueOf(a, "quality", "render_scale") == "100");  // 0.85: not drawn larger
    CHECK(valueOf(a, "quality", "msaa") == "4");
    CHECK(valueOf(a, "quality", "dlss") == "quality");  // 1.3.3: 4K - Quality
    CHECK(a.ui == 200 && valueOf(a, "sound", "engine") == "1" && valueOf(a, "display", "hdr") == "0");
    CHECK(a.ai && a.aiWorld);
    CHECK(valueOf(a, "ai", "enabled") == "1" && valueOf(a, "ai", "ground") == "1");
    CHECK(valueOf(a, "ai", "generate") == "0");
    CHECK(valueOf(a, "ai", "scale") == "4" && valueOf(a, "effects", "day_cycle").empty());  // 1.5.9
    Machine full_hd = rtx5060();
    full_hd.screenWidth = 1920;
    full_hd.screenHeight = 1080;
    CHECK(valueOf(autoSetup(full_hd), "quality", "render_scale") == "100");
    // 1.3.3: an RTX 3060 on a 1080p HDR monitor - DLAA, the interface at 100 %; 3.0.0-beta.1: HDR stays off (the
    // user: off by default, the player turns it on)
    full_hd.adapter = "NVIDIA GeForce RTX 3060";
    full_hd.vramMb = 12288;
    full_hd.hdr = true;
    const AutoSetup b = autoSetup(full_hd);
    CHECK(valueOf(b, "quality", "dlss") == "dlaa" && b.ui == 100 && valueOf(b, "display", "hdr") == "0");
    Machine gtx = full_hd;  // no RTX: no DLSS
    gtx.adapter = "NVIDIA GeForce GTX 1660";
    gtx.vramMb = 6144;
    CHECK(valueOf(autoSetup(gtx), "quality", "dlss").empty());
    CHECK(valueOf(a, "frames", "fps_limit") == "60" && valueOf(a, "frames", "save_power") == "1");
}

TEST(auto_setup_an_old_radeon_on_direct3d_11) {
    // 0.50: a Radeon HD 6570 (1 GB) on Windows 11 - no Direct3D 12, Direct3D 11 at 11_0.
    Machine m;
    m.windows10 = true;
    m.d3d11 = true;
    m.d3d11Level = 0xb000;
    m.vendor = 0x1002;
    m.vramMb = 1024;
    m.screenWidth = 1920;
    m.screenHeight = 1080;
    m.pack = true;
    const AutoSetup a = autoSetup(m);
    CHECK(a.tier == Tier::Old && a.preset == Preset::Speed);
    CHECK(valueOf(a, "effects", "actor_shadows") == "0");  // 0.90.3: costly in a crowd
    CHECK(valueOf(a, "renderer", "api") == "d3d11");
    CHECK(valueOf(a, "quality", "render_scale") == "100");
    // 2.5.16: the world at 75 % of the picture, the interface sharp (R-DUMAMX: 35 frames with every improvement off)
    CHECK(valueOf(a, "quality", "world_scale") == "75");
    CHECK(valueOf(a, "quality", "msaa") == "0");
    CHECK(valueOf(a, "quality", "dlss") == "off" && valueOf(a, "frames", "frame_generation") == "off");
    CHECK(valueOf(a, "renderer", "bridge") == "0");
    CHECK(a.ai && a.aiWorld);  // 0.57: 1 GB and the textures compressed - the world's too
    CHECK(valueOf(a, "ai", "compress") == "auto");
    // 1.5.9: the tester's reports - x2 pictures and no day and night take a fifth of the card's frame
    CHECK(valueOf(a, "ai", "scale") == "2" && valueOf(a, "effects", "day_cycle") == "0");
    m.vramMb = 512;
    const AutoSetup small = autoSetup(m);
    CHECK(small.ai && !small.aiWorld);  // under 1 GB: sprites and icons only
    CHECK(valueOf(small, "ai", "players") == "1" && valueOf(small, "ai", "ground") == "0");
    m.vramMb = 1024;
    m.pack = false;
    CHECK(valueOf(autoSetup(m), "ai", "enabled") == "0");  // no pack: nothing to show
}

TEST(auto_setup_without_a_graphics_driver) {
    Machine m;
    m.windows10 = m.d3d12 = m.d3d11 = true;  // Windows' basic display adapter makes devices - in software
    m.d3d11Level = 0xb100;
    m.vendor = 0x1414;
    m.pack = true;
    AutoSetup a = autoSetup(m);
    CHECK(a.tier == Tier::NoDriver && a.preset == Preset::Plain);
    CHECK(!a.ai && valueOf(a, "ai", "enabled") == "0");
    CHECK(valueOf(a, "display", "high_resolution") == "0");
    Machine dx10;  // Direct3D 10 alone
    dx10.d3d11 = true;
    dx10.d3d11Level = 0xa100;
    dx10.vendor = 0x10de;
    CHECK(autoSetup(dx10).tier == Tier::NoDriver);
    CHECK(autoSetup(Machine()).tier == Tier::NoDriver);
}

TEST(auto_setup_small_and_middle_cards) {
    Machine m = rtx5060();
    m.vendor = 0x8086;  // integrated Intel graphics
    m.vramMb = 128;
    AutoSetup a = autoSetup(m);
    CHECK(a.tier == Tier::Low && a.preset == Preset::Speed);
    CHECK(valueOf(a, "effects", "ao") == "0" && valueOf(a, "effects", "weather") == "0");
    CHECK(valueOf(a, "quality", "dlss") == "off");  // not NVIDIA's
    CHECK(a.ai && !a.aiWorld);
    m.vendor = 0x1002;
    m.vramMb = 6144;
    a = autoSetup(m);
    CHECK(a.tier == Tier::Middle && a.preset == Preset::Balance);
    CHECK(valueOf(a, "quality", "render_scale") == "75");  // 0.58: 4K on a middle card - drawn smaller
    CHECK(valueOf(a, "quality", "sharpen") == "50");
    CHECK(valueOf(a, "effects", "ao").empty());  // the player's choice stays
    CHECK(a.aiWorld);
    CHECK(std::string(tierName(Tier::Old)) == "old");
    m.vramMb = 3072;  // 0.51.1: DXGI's cap for a 32-bit program, the driver's number unknown
    m.vramCapped = true;
    CHECK(autoSetup(m).tier == Tier::Middle);
}

TEST(auto_setup_taa_for_cards_without_dlss) {
    // 0.75: the renderer's TAA where there is no DLSS - a strong AMD card; not NVIDIA's (DLSS), not the old Radeon.
    Machine amd = rtx5060();
    amd.vendor = 0x1002;
    CHECK(valueOf(autoSetup(amd), "quality", "taa") == "1");
    CHECK(valueOf(autoSetup(rtx5060()), "quality", "taa") == "0");
    Machine old = amd;
    old.d3d12 = false;
    old.d3d11Level = 0xb000;
    old.vramMb = 1024;
    CHECK(valueOf(autoSetup(old), "quality", "taa") == "0");
}

TEST(auto_setup_keeps_the_frame_rate_on_smaller_cards) {
    // 0.76: [frames] keep_fps 45 short of a strong card; a strong one decides itself.
    CHECK(valueOf(autoSetup(rtx5060()), "frames", "keep_fps") == "0");
    Machine old = rtx5060();
    old.vendor = 0x1002;
    old.d3d12 = false;
    old.d3d11Level = 0xb000;
    old.vramMb = 1024;
    CHECK(valueOf(autoSetup(old), "frames", "keep_fps") == "45");
}

namespace {

Machine integratedCard(const std::string& name) {  // a laptop's integrated graphics: Windows 11, D3D12, 1080p
    Machine m;
    m.adapter = name;
    m.windows10 = m.d3d12 = m.d3d11 = true;
    m.d3d11Level = 0xc100;
    m.vendor = 0x8086;
    m.vramMb = 128;
    m.integrated = true;
    m.battery = true;
    m.screenWidth = 1920;
    m.screenHeight = 1080;
    m.pack = true;
    return m;
}

}  // namespace

// 2.12: integrated graphics told apart (the page QjTrLqpnyyw8qKiUZQ3i24's tiers): the weak ones get the super-light
// mode, the strong ones the low set with 2x MSAA.
TEST(auto_setup_weak_integrated_is_super_light) {
    const AutoSetup a = autoSetup(integratedCard("Intel(R) UHD Graphics 620"));
    CHECK(a.tier == Tier::IntegratedWeak);
    CHECK_EQ(valueOf(a, "quality", "super_light"), std::string("1"));
    CHECK_EQ(valueOf(a, "effects", "preset"), std::string("off"));
    CHECK_EQ(valueOf(a, "quality", "msaa"), std::string("0"));
    CHECK_EQ(valueOf(a, "frames", "fps_limit"), std::string("60"));
    CHECK_EQ(valueOf(a, "frames", "battery_saver"), std::string("1"));
    CHECK(a.ai && !a.aiWorld && valueOf(a, "ai", "scale") == "2");
    CHECK_EQ(valueOf(a, "quality", "dlss"), std::string("off"));
    Machine desktop = integratedCard("AMD Radeon(TM) Vega 8 Graphics");
    desktop.vendor = 0x1002;
    desktop.battery = false;
    const AutoSetup b = autoSetup(desktop);
    CHECK(b.tier == Tier::IntegratedWeak && valueOf(b, "frames", "battery_saver") == "0");
}

TEST(auto_setup_strong_integrated_is_low_with_msaa_2) {
    for (const char* name : {"Intel(R) Iris(R) Xe Graphics", "Intel(R) Arc(TM) Graphics", "AMD Radeon 780M Graphics",
                             "AMD Radeon(TM) 680M"}) {
        const AutoSetup a = autoSetup(integratedCard(name));
        CHECK(a.tier == Tier::IntegratedStrong);
        CHECK(a.preset == Preset::Speed);
        CHECK_EQ(valueOf(a, "quality", "msaa"), std::string("2"));
        CHECK_EQ(valueOf(a, "effects", "preset"), std::string("weak"));
        CHECK_EQ(valueOf(a, "frames", "keep_fps"), std::string("45"));
        CHECK_EQ(valueOf(a, "quality", "super_light"), std::string("0"));
    }
}

TEST(integrated_by_memory_and_name) {
    CHECK(integratedByMemory(128, 8192));
    CHECK(integratedByMemory(512, 16384));
    CHECK(!integratedByMemory(4096, 8192));  // a card of its own
    CHECK(!integratedByMemory(1024, 0));      // no shared memory said: not told
    CHECK(strongIntegrated("Intel(R) Iris(R) Xe Graphics"));
    CHECK(strongIntegrated("AMD Radeon 890M Graphics"));
    CHECK(!strongIntegrated("Intel(R) Arc(TM) A770 Graphics"));  // a card of its own
    CHECK(!strongIntegrated("Intel(R) UHD Graphics 770"));
}

TEST(auto_setup_takes_the_fast_card_of_a_laptop) {
    // Optimus: the probe's main adapter is the NVIDIA one the game draws on - its own memory, not integrated
    Machine m = integratedCard("NVIDIA GeForce RTX 3060 Laptop GPU");
    m.vendor = 0x10de;
    m.vramMb = 6144;
    m.integrated = false;
    CHECK(autoSetup(m).tier == Tier::Middle);
}

// 2.12: the app's start - a fresh install set up at once, a used one only recorded, a changed card or screen asked.
TEST(first_start_decides_by_the_install_and_the_machine) {
    CHECK(firstStart(false, "", "RTX|8192|1920x1080") == FirstStart::Apply);
    CHECK(firstStart(true, "", "RTX|8192|1920x1080") == FirstStart::Record);
    CHECK(firstStart(true, "RTX|8192|1920x1080", "RTX|8192|1920x1080") == FirstStart::Nothing);
    CHECK(firstStart(true, "RTX|8192|1920x1080", "RTX|8192|2560x1440") == FirstStart::Ask);
    CHECK(firstStart(false, "RTX|8192|1920x1080", "UHD|128|1920x1080") == FirstStart::Ask);
    Machine m = rtx5060();
    CHECK_EQ(machineKey(m), std::string("NVIDIA GeForce RTX 5060 Ti|16384|3840x2160"));
}
