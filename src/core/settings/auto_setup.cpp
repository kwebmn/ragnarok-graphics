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
    set(e, "renderer", "api", a.tier == Tier::Old ? "d3d11" : "auto");
    // 2.5.16: Direct3D 11's world at 75 % of the picture, the interface sharp - an HD 6570 drew 35 frames with every
    // improvement off, the world most of each (R-DUMAMX); the others draw it whole
    set(e, "quality", "world_scale", a.tier == Tier::Old ? "75" : "100");
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
    // 2.2: the Effects page's set by the card - its own switches as the set says
    set(e, "effects", "preset",
        a.tier == Tier::NoDriver ? "off" : a.tier == Tier::Middle ? "medium" : a.tier == Tier::High ? "strong" : "weak");
    // The effects of the Effects page: Direct3D 12's. A small card keeps the light ones.
    if (a.tier == Tier::Low)
        for (const char* k : {"ao", "dof", "rays", "haze", "heat", "weather", "glow"})
            set(e, "effects", k, "0");
    // The AI textures: the pack's pictures only (the game makes none: that is for debugging). The world's take the
    // most video memory: a card under 1 GB keeps to the sprites and the icons (0.57: 2 GB before the compression,
    // [ai] compress = auto, which a card under 3 GB gets).
    // 1.3.3: the interface a whole number of times the game's pixels - a fraction (167 %) softens every letter
    a.ui = m.screenHeight >= 2000 ? 200 : 100;
    // NVIDIA RTX with Direct3D 12: DLAA (the picture at its own size, smoothed by the network) where the screen is up
    // to 2560x1440 - DLSS Quality there draws the world at 720p and it shows; Quality on larger screens.
    const bool rtx = m.vendor == kNvidia && m.adapter.find("RTX") != std::string::npos;
    if (rtx && (a.tier == Tier::High || a.tier == Tier::Middle)) {
        a.dlss = pixels <= 2560LL * 1440 ? "dlaa" : "quality";
        set(e, "quality", "dlss", a.dlss);
    }
    a.hdrOn = m.hdr && (a.tier == Tier::High || a.tier == Tier::Middle);  // Direct3D 12's HDR10 picture
    set(e, "display", "hdr", a.hdrOn ? "1" : "0");
    a.soundEngine = m.windows10;  // XAudio2 2.9 is there
    set(e, "sound", "engine", a.soundEngine ? "1" : "0");
    if (a.soundEngine) set(e, "sound", "headphones", "1");
    a.ai = m.pack && a.tier != Tier::NoDriver;
    a.aiWorld = a.ai && m.vramMb >= 1024;
    set(e, "ai", "enabled", a.ai ? "1" : "0");
    set(e, "ai", "generate", "0");
    set(e, "ai", "compress", "auto");
    // 1.5.9: a small card takes the pack's pictures at twice the game's size, not four times - a Radeon HD 6570 at
    // 1600x891 in Prontera: 25.7 ms of its work a frame with x4, 21.2 with x2 (33 frames a second, 42).
    set(e, "ai", "scale", a.tier == Tier::Old || a.tier == Tier::Low ? "2" : "4");
    // ... and on Direct3D 11 no day and night: about 1 ms of such a card's frame.
    if (a.tier == Tier::Old) set(e, "effects", "day_cycle", "0");
    if (a.ai) {
        for (const char* k : {"monsters", "players", "npcs", "items", "icons"}) set(e, "ai", k, "1");
        for (const char* k : {"ground", "objects", "effects"}) set(e, "ai", k, a.aiWorld ? "1" : "0");
    }
    return a;
}

}  // namespace uagfx::settings
