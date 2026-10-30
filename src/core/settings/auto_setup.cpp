#include "core/settings/auto_setup.hpp"

#include <cstdlib>

namespace uagfx::settings {
namespace {

constexpr unsigned kNvidia = 0x10de;
constexpr unsigned kMicrosoft = 0x1414;  // the Basic Render Driver: Direct3D in software
constexpr unsigned kLevel11 = 0xb000;

Tier tierOf(const Machine& m) {
    if (m.vendor == kMicrosoft) return Tier::NoDriver;
    if (m.windows10 && m.d3d12) {
        if (m.integrated) return strongIntegrated(m.adapter) ? Tier::IntegratedStrong : Tier::IntegratedWeak;
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

std::vector<IniEdit> autopilotOnce(const std::map<std::string, std::string>& ini, int memoryLevel) {
    if (ini.count("utility.autopilot")) return {};
    const auto value = [&](const char* key, const char* absent) {
        const auto it = ini.find(key);
        return it == ini.end() ? std::string(absent) : it->second;
    };
    static const char* const kSets[] = {"off", "weak", "medium", "strong"};
    std::vector<IniEdit> e;
    const std::string preset = value("effects.preset", "auto");
    bool automatic = preset == "auto";
    if (memoryLevel >= 1 && memoryLevel <= 3 && preset == kSets[memoryLevel]) {  // as its setup gave it
        e.push_back({"effects", "preset", "auto"});
        automatic = true;
    }
    const std::string keep = value("frames.keep_fps", "0");
    if (automatic && value("quality.super_light", "0") != "1" && (keep == "0" || keep == "45"))
        e.push_back({"frames", "keep_fps", "60"});
    e.push_back({"utility", "autopilot", "1"});
    return e;
}

std::vector<IniEdit> worldScaleReset(const std::map<std::string, std::string>& ini) {
    if (ini.count("utility.world_scale_reset")) return {};
    std::vector<IniEdit> e;
    const auto it = ini.find("quality.world_scale");
    if (it != ini.end() && std::atoi(it->second.c_str()) < 100) e.push_back({"quality", "world_scale", "100"});
    e.push_back({"utility", "world_scale_reset", "1"});
    return e;
}

const char* tierName(Tier t) {
    switch (t) {
        case Tier::NoDriver: return "no driver";
        case Tier::Old: return "old";
        case Tier::Low: return "low";
        case Tier::Middle: return "middle";
        case Tier::High: return "high";
        case Tier::IntegratedWeak: return "integrated, weak";
        case Tier::IntegratedStrong: return "integrated, strong";
    }
    return "?";
}

std::string machineKey(const Machine& m) {
    return m.adapter + "|" + std::to_string(m.vramMb) + "|" + std::to_string(m.screenWidth) + "x" +
           std::to_string(m.screenHeight);
}

FirstStart firstStart(bool usedBefore, const std::string& kept, const std::string& now, bool playedHere) {
    if (kept.empty()) return usedBefore ? FirstStart::Record : FirstStart::Apply;
    if (kept == now) return FirstStart::Nothing;
    return playedHere ? FirstStart::Ask : FirstStart::Apply;
}

bool integratedByMemory(unsigned long long dedicatedMb, unsigned long long sharedMb) {
    return dedicatedMb <= 1024 && sharedMb > dedicatedMb;
}

bool strongIntegrated(const std::string& adapter) {
    for (const char* n : {"Iris Xe", "Iris(R) Xe", "680M", "760M", "780M", "880M", "890M", "8050S", "8060S"})
        if (adapter.find(n) != std::string::npos) return true;
    // Intel's Arc in Core Ultra is "Arc(TM) Graphics"; its cards of their own have a model: "Arc(TM) A770 Graphics"
    return adapter.find("Arc(TM) Graphics") != std::string::npos || adapter.find("Arc Graphics") != std::string::npos;
}

AutoSetup autoSetup(const Machine& m) {
    AutoSetup a;
    a.tier = tierOf(m);
    switch (a.tier) {
        case Tier::NoDriver: a.preset = Preset::Plain; break;
        case Tier::Old:
        case Tier::Low:
        case Tier::IntegratedWeak:
        case Tier::IntegratedStrong: a.preset = Preset::Speed; break;
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
    const bool small = a.tier == Tier::Low || a.tier == Tier::Old || a.tier == Tier::IntegratedStrong;
    if (small && pixels > 1920LL * 1200) set(e, "quality", "render_scale", "67");
    if (a.tier != Tier::High && pixels > 1920LL * 1200 && a.tier != Tier::NoDriver)
        set(e, "quality", "sharpen", "50");  // the sharp upscale's CAS
    set(e, "renderer", "api", a.tier == Tier::Old ? "d3d11" : "auto");
    // 2.5.16 gave Direct3D 11's old cards the world at 75 % of the picture (R-DUMAMX: an HD 6570 drew 35 frames with
    // every improvement off); 3.0.9-beta.7 (the owner, R-3FS6YX: the character smeared): whole on every card
    set(e, "quality", "world_scale", "100");
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
    // 0.76: a card keeps its frames - the costliest effects go off when it cannot; 3.0.11-beta.58 (the autopilot):
    // every card at 60, the strong one too (an old card of 8 GB is strong by its memory alone)
    set(e, "frames", "keep_fps", a.tier == Tier::NoDriver ? "0" : "60");
    // 2.2: the Effects page's set by the card; 3.0.11-beta.58: the automatic one - the card's memory's to start with,
    // then judged by how it draws (core/card_speed)
    set(e, "effects", "preset", a.tier == Tier::NoDriver ? "off" : "auto");
    // The effects of the Effects page: Direct3D 12's. A small card keeps the light ones.
    if (a.tier == Tier::Low || a.tier == Tier::IntegratedStrong)
        for (const char* k : {"ao", "rays", "heat", "weather", "glow"})
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
    // 3.0.0-beta.1 (the user): HDR off by default, whatever the monitor - the player turns it on (the Main page)
    set(e, "display", "hdr", "0");
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
    const bool weak = a.tier == Tier::IntegratedWeak;
    set(e, "ai", "scale", small || weak ? "2" : "4");
    // ... and on Direct3D 11 no day and night: about 1 ms of such a card's frame.
    if (a.tier == Tier::Old) set(e, "effects", "day_cycle", "0");
    // 2.12: integrated graphics - the weak ones the super-light mode at 60 frames (the battery spared on a laptop),
    // the strong ones 2x MSAA on the low set
    set(e, "quality", "super_light", weak ? "1" : "0");
    if (weak) {
        set(e, "effects", "preset", "off");
        set(e, "quality", "msaa", "0");
        set(e, "frames", "fps_limit", "60");
        set(e, "frames", "keep_fps", "0");
        set(e, "frames", "battery_saver", m.battery ? "1" : "0");
    }
    if (a.tier == Tier::IntegratedStrong) set(e, "quality", "msaa", "2");
    if (a.ai) {
        for (const char* k : {"monsters", "players", "npcs", "items", "icons"}) set(e, "ai", k, "1");
        for (const char* k : {"ground", "objects", "effects"}) set(e, "ai", k, a.aiWorld ? "1" : "0");
    }
    return a;
}

}  // namespace uagfx::settings
