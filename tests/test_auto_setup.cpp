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
    CHECK(valueOf(a, "quality", "render_scale") == "150");  // 4K: not twice
    CHECK(valueOf(a, "quality", "msaa") == "4");
    CHECK(valueOf(a, "quality", "dlss").empty());  // NVIDIA's own choice stays
    CHECK(a.ai && a.aiWorld);
    CHECK(valueOf(a, "ai", "enabled") == "1" && valueOf(a, "ai", "ground") == "1");
    CHECK(valueOf(a, "ai", "generate") == "0");
    Machine full_hd = rtx5060();
    full_hd.screenWidth = 1920;
    full_hd.screenHeight = 1080;
    CHECK(valueOf(autoSetup(full_hd), "quality", "render_scale") == "200");
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
    CHECK(valueOf(a, "renderer", "api") == "d3d11");
    CHECK(valueOf(a, "quality", "render_scale") == "100");
    CHECK(valueOf(a, "quality", "msaa") == "0");
    CHECK(valueOf(a, "quality", "dlss") == "off" && valueOf(a, "frames", "frame_generation") == "off");
    CHECK(valueOf(a, "renderer", "bridge") == "0");
    CHECK(a.ai && a.aiWorld);  // 0.57: 1 GB and the textures compressed - the world's too
    CHECK(valueOf(a, "ai", "compress") == "auto");
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
