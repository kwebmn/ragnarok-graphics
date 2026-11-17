#include "core/render_place.hpp"

#include <atomic>

#include "core/bridge_protocol.hpp"

namespace uagfx::renderplace {
namespace {

std::atomic<StepSink> g_sink{nullptr};

}  // namespace

const char* stepName(uint32_t s) {
    switch (s) {
        case kGeometry: return "its vertices and indices put up";
        case kUploads: return "the game's textures written";
        case kDraws: return "its draws";
        case kAfterDraws: return "the casters and the own ground's notes";
        case kSubmit: return "the list to the GPU";
        case kLooks: return "the textures' looks worked out";
        case kCompose: return "the picture put together";
        case kShow: return "the swap chain's present";
        default: return "";
    }
}

const char* messageName(uint32_t message) {
    using bridge::Msg;
    switch (Msg(message)) {
        case Msg::SetUiSharp: return "SetUiSharp";
        case Msg::PreparePipelines: return "PreparePipelines";
        case Msg::SetTarget: return "SetTarget";
        case Msg::SetScale: return "SetScale";
        case Msg::SetDrawQuality: return "SetDrawQuality";
        case Msg::CreateTexture: return "CreateTexture";
        case Msg::ReleaseTexture: return "ReleaseTexture";
        case Msg::Execute: return "Execute";
        case Msg::ReadTarget: return "ReadTarget";
        case Msg::Present: return "Present";
        case Msg::SetSurface: return "SetSurface";
        case Msg::Camera: return "Camera";
        case Msg::Quit: return "Quit";
        case Msg::Map: return "Map";
        case Msg::IconWanted: return "IconWanted";
        case Msg::SetEffects: return "SetEffects";
        case Msg::ObjectTexture: return "ObjectTexture";
        case Msg::SetStudio: return "SetStudio";
        case Msg::SetWorldScale: return "SetWorldScale";
        case Msg::SetLight: return "SetLight";
        case Msg::SetProfile: return "SetProfile";
        case Msg::ProfileCut: return "ProfileCut";
        case Msg::SeeThrough: return "SeeThrough";
        case Msg::GameModel: return "GameModel";
        case Msg::GameModelsFrame: return "GameModelsFrame";
        default: return "";
    }
}

std::string describe(const Place& p, uint32_t nowMs) {
    const auto name = [](uint32_t m) {
        const char* n = messageName(m);
        return *n ? std::string(n) : "message " + std::to_string(m);
    };
    std::string s;
    if (p.message) {
        s = "in " + name(p.message) + " for " + std::to_string(nowMs - p.sinceMs) + " ms";
        if (*stepName(p.step)) s += std::string(" (") + stepName(p.step) + ")";
    } else {
        s = "between messages";
    }
    s += "; before it: ";
    bool any = false;
    for (const uint32_t m : p.done) {
        if (!m) break;
        s += (any ? ", " : "") + name(m);
        any = true;
    }
    if (!any) s += "none";
    return s;
}

void setStepSink(StepSink sink) { g_sink.store(sink); }

void step(uint32_t s) {
    if (const StepSink sink = g_sink.load()) sink(s);
}

}  // namespace uagfx::renderplace
