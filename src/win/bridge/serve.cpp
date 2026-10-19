#include "win/bridge/serve.hpp"

#include <cstring>
#include <string>

#include "core/bridge_wire.hpp"
#include "core/gpu_timing.hpp"
#include "win/log.hpp"

namespace uagfx::win::bridge {

using uagfx::bridge::WireReader;
using uagfx::bridge::WireWriter;

bool Server::dispatch(Msg type, const uint8_t* data, uint32_t size) {
    WireReader rd(data, size);
    out_.clear();
    WireWriter w(out_);
    switch (type) {
        case Msg::SetUiSharp: renderer_.setUiSharp(rd.u8() != 0); break;
        case Msg::PreparePipelines: renderer_.preparePipelines(rd.str()); break;
        case Msg::SetTarget: {
            gameWidth_ = rd.i32();
            gameHeight_ = rd.i32();
            w.u8(rd.ok() && renderer_.setTarget(gameWidth_, gameHeight_) ? 1 : 0);
            channel_.send(Msg::TargetSet, out_);
            break;
        }
        case Msg::SetScale: {
            const double k = rd.f64();
            const int width = rd.i32();
            const int height = rd.i32();
            const int samples = rd.i32();
            const bool upscale = rd.u8() != 0;
            w.u8(rd.ok() && renderer_.setScale(k, width, height, samples, upscale) ? 1 : 0);
            w.i32(renderer_.samples());
            channel_.send(Msg::ScaleSet, out_);
            break;
        }
        case Msg::SetStudio: {  // 2.3
            uagfx::bridge::StudioKeys k;
            if (uagfx::bridge::readStudioKeys(rd, k)) renderer_.setStudio(k.flags, k.x, k.y);
            break;
        }
        case Msg::SetDrawQuality: {
            const int aniso = rd.i32();
            const bool coverage = rd.u8() != 0;
            const uint8_t aiKinds = rd.u8();  // 2.2.1: the kinds shown
            const uint8_t aiSprites = rd.u8();  // 2.2.2: the sprites' kinds shown
            renderer_.setDrawQuality(aniso, coverage, aiKinds, aiSprites);
            break;
        }
        case Msg::SetEffects: {
            EffectSettings e;
            e.glow = rd.u8() != 0;
            e.glowStrength = rd.i32();
            e.glowSize = rd.i32();
            e.glowLight = rd.i32();
            e.ao = rd.u8() != 0;
            e.aoStrength = rd.i32();
            e.aoSize = rd.i32();
            e.water = rd.u8() != 0;
            e.waterWaves = rd.i32();
            e.waterReflections = rd.i32();
            e.waterClarity = rd.i32();
            e.grade = rd.u8() != 0;
            e.gradeSaturation = rd.i32();
            e.gradeContrast = rd.i32();
            e.gradeBrightness = rd.i32();
            e.gradeWarmth = rd.i32();
            e.gradeVignette = rd.i32();
            e.smoothLight = rd.u8() != 0;
            e.torch = rd.u8() != 0;
            e.torchGlow = rd.i32();
            e.torchLight = rd.i32();
            e.torchFlicker = rd.i32();
            e.torchFlame = rd.u8() != 0;
            e.torchBrightness = rd.i32();
            e.smoke = rd.u8() != 0;
            e.smokeDensity = rd.i32();
            e.lamps = rd.u8() != 0;
            e.lampsLight = rd.i32();
            e.fireLight = rd.u8() != 0;
            e.fireLightStrength = rd.i32();
            e.fireflies = rd.u8() != 0;
            e.firefliesBrightness = rd.i32();
            e.mapGlows = rd.u8() != 0;
            e.mapGlowsBrightness = rd.i32();
            e.rays = rd.u8() != 0;
            e.raysBrightness = rd.i32();
            e.lava = rd.u8() != 0;
            e.lavaBrightness = rd.i32();
            e.actorShadows = rd.u8() != 0;
            e.actorShadowsStrength = rd.i32();
            e.heat = rd.u8() != 0;
            e.heatStrength = rd.i32();
            e.skillColours = rd.u8() != 0;
            e.portals = rd.u8() != 0;
            e.portalsStrength = rd.i32();
            e.weather = rd.u8() != 0;
            e.weatherAmount = rd.i32();
            e.night = rd.u8() != 0;  // 0.63
            e.nightStrength = rd.i32();
            e.nightClock = rd.u8() != 0;
            e.nightNow = rd.i32();
            e.foliage = rd.u8() != 0;  // 0.64
            e.foliageStrength = rd.i32();
            e.softParticles = rd.u8() != 0;  // 0.67
            e.rimLight = rd.u8() != 0;  // 0.68
            e.rimStrength = rd.i32();
            e.sky = rd.u8() != 0;  // 0.69
            e.skyClouds = rd.i32();
            e.skyStrength = rd.i32();
            e.motes = rd.u8() != 0;  // 0.70
            e.motesAmount = rd.i32();
            e.relief = rd.u8() != 0;  // 0.71
            e.reliefStrength = rd.i32();
            e.floorReflections = rd.u8() != 0;  // 0.72
            e.floorReflectStrength = rd.i32();
            e.shafts = rd.u8() != 0;  // 0.73
            e.shaftsStrength = rd.i32();
            e.dayCycle = rd.u8() != 0;  // 0.74
            e.dayNow = rd.i32();
            e.bloom = rd.u8() != 0;  // 0.78
            e.bloomStrength = rd.i32();
            e.regionLooks = rd.u8() != 0;  // 0.79
            e.regionLooksStrength = rd.i32();
            e.footprints = rd.u8() != 0;  // 0.80
            e.storms = rd.u8() != 0;
            e.grass = rd.u8() != 0;  // 0.81
            e.grassAmount = rd.i32();
            e.skillSparks = rd.u8() != 0;  // 0.82
            e.skillSparksAmount = rd.i32();
            e.filmGrain = rd.u8() != 0;  // 0.83
            e.filmGrainAmount = rd.i32();
            e.mapFade = rd.u8() != 0;
            e.noGameFog = rd.u8() != 0;  // 2.6.10
            e.roundLight = rd.u8() != 0;  // 0.84
            e.appear = rd.u8() != 0;  // 0.93
            e.ripples = rd.u8() != 0;  // 0.94
            e.glint = rd.u8() != 0;  // 0.95
            e.hitFlash = rd.u8() != 0;  // 0.96
            e.wetGround = rd.u8() != 0;  // 0.97
            e.flare = rd.u8() != 0;  // 0.98
            e.treeVolume = rd.u8() != 0;  // 1.7
            e.leafLight = rd.u8() != 0;
            e.treeTints = rd.u8() != 0;
            e.gusts = rd.u8() != 0;
            e.dapple = rd.u8() != 0;
            e.flowers = rd.u8() != 0;  // 1.7.1
            e.dew = rd.u8() != 0;
            e.morningFog = rd.u8() != 0;
            e.sandDrift = rd.u8() != 0;
            e.snowSparkle = rd.u8() != 0;
            e.wetWalls = rd.u8() != 0;
            e.shoreFoam = rd.u8() != 0;  // 1.7.2
            e.fish = rd.u8() != 0;
            e.swamp = rd.u8() != 0;
            e.butterflies = rd.u8() != 0;  // 1.7.3
            e.birds = rd.u8() != 0;
            e.leafFall = rd.u8() != 0;
            e.embers = rd.u8() != 0;
            e.breath = rd.u8() != 0;
            e.splashes = rd.u8() != 0;
            e.caveDrips = rd.u8() != 0;
            e.underwater = rd.u8() != 0;
            e.materials = rd.u8() != 0;  // 1.8
            e.materialsStrength = rd.i32();
            e.tier = rd.i32();
            e.groundSeams = rd.u8() != 0;  // 1.9
            e.smoothHills = rd.u8() != 0;  // 1.15
            e.smoothHillsStrength = rd.i32();
            e.worldReflections = rd.u8() != 0;  // 1.16
            e.spriteLight = rd.u8() != 0;  // 1.17
            e.spriteLightStrength = rd.i32();
            e.edgeLand = rd.u8() != 0;  // 1.18
            e.seasons = rd.u8() != 0;  // 1.20
            e.rainWet = rd.u8() != 0;
            e.nightWindows = rd.u8() != 0;  // 1.21
            e.sharedWind = rd.u8() != 0;  // 1.22
            e.litDust = rd.u8() != 0;  // 1.23
            // 2.4: the parts of the effects, in Remote::setEffects's order
            e.waterFoam = rd.u8() != 0;
            e.waterGlints = rd.u8() != 0;
            e.caustics = rd.u8() != 0;
            e.waterfalls = rd.u8() != 0;
            e.wetSand = rd.u8() != 0;
            e.stepDust = rd.u8() != 0;
            e.seasonSnow = rd.u8() != 0;
            e.seasonGrass = rd.u8() != 0;
            e.pollen = rd.u8() != 0;
            e.indoorDust = rd.u8() != 0;
            e.rainSplashes = rd.u8() != 0;
            e.stars = rd.u8() != 0;
            e.desertHeat = rd.u8() != 0;
            e.skyRays = rd.u8() != 0;
            e.treeSeasons = rd.u8() != 0;
            e.dragonflies = rd.u8() != 0;
            if (rd.ok()) renderer_.setEffects(e);
            break;
        }
        case Msg::CreateTexture: {
            const uint32_t id = rd.u32();
            const int width = rd.i32();
            const int height = rd.i32();
            const int levels = rd.i32();
            if (rd.ok() && !renderer_.createTextureAs(id, width, height, levels))
                LOG_ERROR("texture %u (%dx%d, %d levels) not made", id, width, height, levels);
            break;
        }
        case Msg::ReleaseTexture: renderer_.releaseTexture(rd.u32()); break;
        case Msg::ObjectTexture: {
            const uint32_t id = rd.u32();
            const uint8_t kind = rd.u8();
            if (rd.ok()) renderer_.noteObjectTexture(id, kind);
            break;
        }
        case Msg::Execute: {
            if (uagfx::bridge::readRecord(rd, record_)) {
                renderer_.execute(record_);
            } else {
                static bool logged = false;
                if (!logged) LOG_ERROR("bridge: a damaged frame record was skipped");
                logged = true;
            }
            break;
        }
        case Msg::ReadTarget: {
            const bool ok = renderer_.readTarget(pixels_) && pixels_.size() == size_t(gameWidth_) * gameHeight_;
            w.u8(ok ? 1 : 0);
            w.i32(gameWidth_);
            w.i32(gameHeight_);
            if (ok) w.bytes(pixels_.data(), pixels_.size() * 4);
            channel_.send(Msg::TargetPixels, out_);
            break;
        }
        case Msg::Camera: {
            const bool valid = rd.u8() != 0;
            float view[12];
            for (float& v : view) v = rd.f32();
            const float targetZ = rd.left() >= 4 ? rd.f32() : 0.0f;  // 1.4.2: the character's depth
            if (rd.ok()) renderer_.setCamera(valid, view, targetZ);
            break;
        }
        case Msg::IconWanted: {
            const uint64_t key = rd.u64();
            const int w = rd.i32(), h = rd.i32();
            if (!rd.ok() || w <= 0 || h <= 0 || w > 512 || h > 512) break;
            const uint8_t* bytes = rd.take(size_t(w) * size_t(h) * 4);
            if (!bytes) break;
            std::vector<uint32_t> argb(size_t(w) * size_t(h));
            std::memcpy(argb.data(), bytes, argb.size() * 4);
            renderer_.requestIcon(key, w, h, argb.data());
            break;
        }
        case Msg::SetWorldScale: {  // 2.5.16
            const double share = rd.f64();
            if (rd.ok()) renderer_.setWorldScale(share);
            break;
        }
        case Msg::SetLight: {  // 2.5.20
            const uint8_t flags = rd.u8();
            if (rd.ok()) renderer_.setLight(flags);
            break;
        }
        case Msg::Map: {
            const std::string map = rd.str();
            if (rd.ok()) renderer_.setMap(map);
            break;
        }
        default: return false;
    }
    return true;
}

bool Server::readPresent(const uint8_t* data, uint32_t size, Present& out) {
    WireReader rd(data, size);
    out.window = HWND(uintptr_t(rd.u64()));
    out.width = rd.i32();
    out.height = rd.i32();
    out.source = rd.rect();
    out.options.vsync = rd.u8() != 0;
    out.options.allowTearing = rd.u8() != 0;
    out.options.fpsLimit = rd.i32();
    out.options.sharpen = rd.f32();
    out.options.foreground = rd.u8() != 0;
    out.options.screenshot = rd.str();  // 0.54
    out.options.hdr = rd.u8() != 0;     // 0.65
    out.options.pacerSleeps = rd.u8() != 0;  // 2.5.20
    return rd.ok();
}

void Server::sendTiming() {
    GpuTiming t;  // 2.2.1: the card's work, each new half second's to the game (its frame counter, the console)
    if (!renderer_.gpuTiming(t) || t.serial == timingSent_) return;
    timingSent_ = t.serial;
    timingOut_.clear();
    WireWriter w(timingOut_);
    writeTiming(w, t);
    channel_.trySend(Msg::Timing, timingOut_);
}

void Server::sendLost() {
    if (lostSent_ || !renderer_.lost()) return;
    lostSent_ = true;
    out_.clear();
    WireWriter(out_).str("the GPU device was removed");
    channel_.send(Msg::Lost, out_);
}

}  // namespace uagfx::win::bridge
