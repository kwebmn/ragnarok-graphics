#include "core/settings/auto_setup.hpp"

namespace uagfx::settings {
namespace {

constexpr unsigned kNvidia = 0x10de;
constexpr unsigned kMicrosoft = 0x1414;  // the Basic Render Driver: Direct3D in software
constexpr unsigned kLevel11 = 0xb000;

Tier tierOf(const Machine& m) {
    if (m.vendor == kMicrosoft) return Tier::NoDriver;
    if (m.windows10 && m.d3d12) {
        if (m.vramCapped) return Tier::Middle;  // 3 GB or more, how much more unknown
        if (m.vramMb < 4096) return Tier::Low;
        return m.vramMb < 8192 ? Tier::Middle : Tier::High;
    }
    // Direct3D 11 at 11_0 or better: the renderer of 0.46. A card of Direct3D 10 has a driver as old: the game's own.
    return m.d3d11 && m.d3d11Level >= kLevel11 ? Tier::Old : Tier::NoDriver;
}

void set(std::vector<IniEdit>& e, const char* section, const char* key, const std::string& value) {
    for (IniEdit& x : e)
        if (x.section == section && x.key == key) {
            x.value = value;
            return;
        }
    e.push_back({section, key, value});
}

}  // namespace

const char* tierName(Tier t) {
    switch (t) {
        case Tier::NoDriver: return "no driver";
        case Tier::Old: return "old";
        case Tier::Low: return "low";
        case Tier::Middle: return "middle";
        case Tier::High: return "high";
    }
    return "?";
}

AutoSetup autoSetup(const Machine& m) {
    AutoSetup a;
    a.tier = tierOf(m);
    switch (a.tier) {
        case Tier::NoDriver: a.preset = Preset::Plain; break;
        case Tier::Old:
        case Tier::Low: a.preset = Preset::Speed; break;
        case Tier::Middle: a.preset = Preset::Balance; break;
        case Tier::High: a.preset = Preset::Balance; break;
    }
    std::vector<IniEdit>& e = a.edits;
    e = presetEdits(a.preset);
    if (a.tier == Tier::High) {  // drawn larger and made smaller again: a screen past 2560x1440 asks a lot of it
        const long long pixels = static_cast<long long>(m.screenWidth) * m.screenHeight;
        set(e, "quality", "render_scale", pixels <= 2560LL * 1440 ? "200" : "150");
    }
    set(e, "renderer", "backend", "uaro");
    set(e, "renderer", "api", a.tier == Tier::Old ? "d3d11" : "auto");
    set(e, "renderer", "remix", "0");
    // DLSS and its frame generation: NVIDIA's (the 64-bit bridge brings them); elsewhere off.
    if (m.vendor != kNvidia || a.tier == Tier::Old || a.tier == Tier::NoDriver) {
        set(e, "renderer", "bridge", "0");
        set(e, "quality", "dlss", "off");
        set(e, "frames", "frame_generation", "off");
    }
    // The effects of the Effects page: Direct3D 12's. A small card keeps the light ones.
    if (a.tier == Tier::Low)
        for (const char* k : {"ao", "dof", "clouds", "rays", "haze", "heat", "weather", "glow"})
            set(e, "effects", k, "0");
    // The AI textures: the pack's pictures only (the game makes none: that is for debugging). The world's take the
    // most video memory: a card under 2 GB keeps to the sprites and the icons.
    a.ai = m.pack && a.tier != Tier::NoDriver;
    a.aiWorld = a.ai && m.vramMb >= 2048;
    set(e, "ai", "enabled", a.ai ? "1" : "0");
    set(e, "ai", "generate", "0");
    if (a.ai) {
        for (const char* k : {"monsters", "players", "npcs", "items", "icons"}) set(e, "ai", k, "1");
        for (const char* k : {"ground", "objects", "effects"}) set(e, "ai", k, a.aiWorld ? "1" : "0");
    }
    return a;
}

}  // namespace uagfx::settings
