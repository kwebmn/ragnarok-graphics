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
    const long long pixels = static_cast<long long>(m.screenWidth) * m.screenHeight;
    // 0.85: no card is given the world drawn larger and made smaller (render_scale over 100 %: 150 % at 4K was 2.25
    // times the pixels for an edge a little smoother) - MSAA and the sharpening do it for far less.
    // 0.58: a large screen for a smaller card - drawn smaller and made larger sharply (render_scale under 100 %)
    if (a.tier == Tier::Middle && pixels > 2560LL * 1440) set(e, "quality", "render_scale", "75");
    if ((a.tier == Tier::Low || a.tier == Tier::Old) && pixels > 1920LL * 1200) set(e, "quality", "render_scale", "67");
    if (a.tier != Tier::High && pixels > 1920LL * 1200 && a.tier != Tier::NoDriver)
        set(e, "quality", "sharpen", "50");  // the sharp upscale's CAS
    set(e, "renderer", "backend", "uaro");
    set(e, "renderer", "api", a.tier == Tier::Old ? "d3d11" : "auto");
    set(e, "renderer", "remix", "0");
    // DLSS and its frame generation: NVIDIA's (the 64-bit bridge brings them); elsewhere off.
    if (m.vendor != kNvidia || a.tier == Tier::Old || a.tier == Tier::NoDriver) {
        set(e, "renderer", "bridge", "0");
        set(e, "quality", "dlss", "off");
        set(e, "frames", "frame_generation", "off");
    }
    // 0.75: TAA where there is no DLSS - a strong card, or a middle one on a screen up to 2560x1440 (it draws the
    // world at the screen's size).
    const bool taa =
        m.vendor != kNvidia && (a.tier == Tier::High || (a.tier == Tier::Middle && pixels <= 2560LL * 1440));
    set(e, "quality", "taa", taa ? "1" : "0");
    // 0.76: a card short of a strong one keeps 45 frames a second - the costliest effects go off when it cannot
    const bool keep = a.tier == Tier::Old || a.tier == Tier::Low || a.tier == Tier::Middle;
    set(e, "frames", "keep_fps", keep ? "45" : "0");
    // The effects of the Effects page: Direct3D 12's. A small card keeps the light ones.
    if (a.tier == Tier::Low)
        for (const char* k : {"ao", "dof", "clouds", "rays", "haze", "heat", "weather", "glow"})
            set(e, "effects", k, "0");
    // The AI textures: the pack's pictures only (the game makes none: that is for debugging). The world's take the
    // most video memory: a card under 1 GB keeps to the sprites and the icons (0.57: 2 GB before the compression,
    // [ai] compress = auto, which a card under 3 GB gets).
    a.ai = m.pack && a.tier != Tier::NoDriver;
    a.aiWorld = a.ai && m.vramMb >= 1024;
    set(e, "ai", "enabled", a.ai ? "1" : "0");
    set(e, "ai", "generate", "0");
    set(e, "ai", "compress", "auto");
    if (a.ai) {
        for (const char* k : {"monsters", "players", "npcs", "items", "icons"}) set(e, "ai", k, "1");
        for (const char* k : {"ground", "objects", "effects"}) set(e, "ai", k, a.aiWorld ? "1" : "0");
    }
    return a;
}

}  // namespace uagfx::settings
