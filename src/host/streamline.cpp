#include "host/streamline.hpp"

#include <algorithm>
#include <cstring>
#include <map>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "sl.h"
#include "sl_dlss.h"
#include "sl_dlss_g.h"
#include "sl_pcl.h"
#include "sl_reflex.h"
#pragma GCC diagnostic pop

#include "win/log.hpp"

// Streamline's functions live in sl.interposer.dll, loaded when DLSS is asked for; the SDK's headers call them by
// name (and their inline helpers through slGetFeatureFunction), so they are defined here and forwarded.
namespace {

HMODULE g_sl = nullptr;
PFun_slInit* p_init = nullptr;
PFun_slShutdown* p_shutdown = nullptr;
PFun_slIsFeatureSupported* p_supported = nullptr;
PFun_slSetD3DDevice* p_device = nullptr;
PFun_slGetNewFrameToken* p_token = nullptr;
PFun_slSetConstants* p_constants = nullptr;
PFun_slSetTagForFrame* p_tags = nullptr;
PFun_slEvaluateFeature* p_evaluate = nullptr;
PFun_slGetFeatureFunction* p_function = nullptr;

template <typename F>
F* function(const char* name) {
    return reinterpret_cast<F*>(reinterpret_cast<void*>(GetProcAddress(g_sl, name)));
}

constexpr sl::Result kNotLoaded = sl::Result::eErrorNotInitialized;

// What goes wrong in a frame goes wrong in every frame: a message the first few times, then silence, so the logs
// stay readable and the bridge to the driver is not flooded. Streamline logs from a thread of its own.
struct Seen {
    CRITICAL_SECTION lock;
    std::map<std::string, int> counts;
    Seen() { InitializeCriticalSection(&lock); }
};
constexpr int kRepeats = 3;

// How many times `key` came, this time included; 0 once too many different messages have come.
int seenCount(const std::string& key) {
    static Seen s;
    EnterCriticalSection(&s.lock);
    int n = 0;
    const auto it = s.counts.find(key);
    if (it != s.counts.end()) {
        n = it->second < kRepeats + 1 ? ++it->second : kRepeats + 1;
    } else if (s.counts.size() < 256) {
        s.counts.emplace(key, 1);
        n = 1;
    }
    LeaveCriticalSection(&s.lock);
    return n;
}

void logRepeating(uagfx::LogLevel level, const std::string& key, const std::string& text) {
    const int n = seenCount(key);
    if (n == 0 || n > kRepeats) return;
    if (n < kRepeats)
        uagfx::wlog::line(level, "%s", text.c_str());
    else
        uagfx::wlog::line(level, "%s (again and again: not logged any more)", text.c_str());
}

void slLog(sl::LogType type, const char* text) {
    if (type == sl::LogType::eInfo) return;
    std::string line = text ? text : "";
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    // "[time][streamline][error][tid][uptime]file:line[function] message": the same message without its stamps
    size_t from = 0;
    while (from < line.size() && line[from] == '[') {
        const size_t end = line.find(']', from);
        if (end == std::string::npos) break;
        from = end + 1;
    }
    logRepeating(uagfx::LogLevel::Info, line.substr(from), "streamline: " + line);
}

void failed(const std::string& text) { logRepeating(uagfx::LogLevel::Error, text, text); }

// The paths come from the ANSI command line and module name: the system's code page, not UTF-8 (0.8 review I1).
std::wstring wide(const std::string& s) {
    std::wstring w(s.size() + 1, L'\0');
    const int n = MultiByteToWideChar(CP_ACP, 0, s.c_str(), int(s.size()), &w[0], int(w.size()));
    w.resize(n > 0 ? size_t(n) : 0);
    return w;
}

}  // namespace

extern "C" {
sl::Result slInit(const sl::Preferences& pref, uint64_t sdkVersion) {
    return p_init ? p_init(pref, sdkVersion) : kNotLoaded;
}
sl::Result slShutdown() { return p_shutdown ? p_shutdown() : kNotLoaded; }
sl::Result slIsFeatureSupported(sl::Feature feature, const sl::AdapterInfo& adapter) {
    return p_supported ? p_supported(feature, adapter) : kNotLoaded;
}
sl::Result slSetD3DDevice(void* device) { return p_device ? p_device(device) : kNotLoaded; }
sl::Result slGetNewFrameToken(sl::FrameToken*& token, const uint32_t* frameIndex) {
    return p_token ? p_token(token, frameIndex) : kNotLoaded;
}
sl::Result slSetConstants(const sl::Constants& values, const sl::FrameToken& frame,
                          const sl::ViewportHandle& viewport) {
    return p_constants ? p_constants(values, frame, viewport) : kNotLoaded;
}
sl::Result slSetTagForFrame(const sl::FrameToken& frame, const sl::ViewportHandle& viewport,
                            const sl::ResourceTag* resources, uint32_t count, sl::CommandBuffer* list) {
    return p_tags ? p_tags(frame, viewport, resources, count, list) : kNotLoaded;
}
sl::Result slEvaluateFeature(sl::Feature feature, const sl::FrameToken& frame, const sl::BaseStructure** inputs,
                             uint32_t count, sl::CommandBuffer* list) {
    return p_evaluate ? p_evaluate(feature, frame, inputs, count, list) : kNotLoaded;
}
sl::Result slGetFeatureFunction(sl::Feature feature, const char* name, void*& fn) {
    return p_function ? p_function(feature, name, fn) : kNotLoaded;
}
}

namespace uagfx::host {
namespace {

const sl::ViewportHandle kViewport{0};

struct Mode {
    const char* token;
    sl::DLSSMode mode;
    const char* name;
};
const Mode kModes[] = {{"dlaa", sl::DLSSMode::eDLAA, "DLAA"},
                       {"quality", sl::DLSSMode::eMaxQuality, "DLSS Quality"},
                       {"balanced", sl::DLSSMode::eBalanced, "DLSS Balanced"},
                       {"performance", sl::DLSSMode::eMaxPerformance, "DLSS Performance"},
                       {"ultra", sl::DLSSMode::eUltraPerformance, "DLSS Ultra Performance"}};

std::string result(const char* what, sl::Result r) {
    return std::string(what) + " failed (" + std::to_string(int(r)) + ")";
}

}  // namespace

Dlss::~Dlss() {
    if (started_) slShutdown();
    if (g_sl) FreeLibrary(g_sl);
    g_sl = nullptr;
}

HMODULE Dlss::module() const { return g_sl; }

bool Dlss::init(const std::string& folder, const std::string& logFolder, const std::string& mode, int generation,
                std::string& error) {
    generation_ = generation >= 2 && generation <= 4 ? generation : 0;
    for (const Mode& m : kModes)
        if (mode == m.token) {
            mode_ = uint32_t(m.mode);
            name_ = m.name;
        }
    if (!mode_) {
        error = "no DLSS mode \"" + mode + "\"";
        return false;
    }
    const std::wstring dir = wide(folder);
    g_sl = LoadLibraryW((dir + L"\\sl.interposer.dll").c_str());
    if (!g_sl) {
        error = "cannot load " + folder + "\\sl.interposer.dll (error " + std::to_string(GetLastError()) + ")";
        return false;
    }
    p_init = function<PFun_slInit>("slInit");
    p_shutdown = function<PFun_slShutdown>("slShutdown");
    p_supported = function<PFun_slIsFeatureSupported>("slIsFeatureSupported");
    p_device = function<PFun_slSetD3DDevice>("slSetD3DDevice");
    p_token = function<PFun_slGetNewFrameToken>("slGetNewFrameToken");
    p_constants = function<PFun_slSetConstants>("slSetConstants");
    p_tags = function<PFun_slSetTagForFrame>("slSetTagForFrame");
    p_evaluate = function<PFun_slEvaluateFeature>("slEvaluateFeature");
    p_function = function<PFun_slGetFeatureFunction>("slGetFeatureFunction");
    if (!p_init || !p_shutdown || !p_supported || !p_device || !p_token || !p_constants || !p_tags || !p_evaluate ||
        !p_function) {
        error = "sl.interposer.dll lacks a function this renderer needs";
        return false;
    }
    const std::wstring logs = wide(logFolder);
    const wchar_t* paths[1] = {dir.c_str()};
    // Frame generation needs Reflex and its latency markers (PCL) as well.
    sl::Feature features[4] = {sl::kFeatureDLSS, sl::kFeatureDLSS_G, sl::kFeatureReflex, sl::kFeaturePCL};
    sl::Preferences pref{};
    pref.showConsole = false;
    pref.logLevel = sl::LogLevel::eDefault;
    pref.pathsToPlugins = paths;
    pref.numPathsToPlugins = 1;
    pref.pathToLogsAndData = logs.c_str();
    pref.logMessageCallback = slLog;
    // The renderer sets its state again itself; no OTA. slSetTagForFrame needs frame-based tagging switched on
    // (without it Streamline refuses every tag: 0.9 never upscaled).
    pref.flags = sl::PreferenceFlags::eDisableCLStateTracking | sl::PreferenceFlags::eUseFrameBasedResourceTagging;
    pref.featuresToLoad = features;
    pref.numFeaturesToLoad = generation_ ? 4 : 1;
    pref.engine = sl::EngineType::eCustom;
    pref.engineVersion = "uaRO Graphics";
    pref.projectId = "5b0c8a37-2f6e-4d91-a6c4-0e7f3b9d2c18";
    pref.renderAPI = sl::RenderAPI::eD3D12;
    const sl::Result r = slInit(pref, sl::kSDKVersion);
    if (r != sl::Result::eOk) {
        error = result("slInit", r);
        return false;
    }
    started_ = true;
    return true;
}

bool Dlss::attach(ID3D12Device* device, std::string& error) {
    sl::Result r = slSetD3DDevice(device);
    if (r != sl::Result::eOk) {
        error = result("slSetD3DDevice", r);
        return false;
    }
    LUID luid = device->GetAdapterLuid();
    sl::AdapterInfo adapter{};
    adapter.deviceLUID = reinterpret_cast<uint8_t*>(&luid);
    adapter.deviceLUIDSizeInBytes = sizeof luid;
    r = slIsFeatureSupported(sl::kFeatureDLSS, adapter);
    if (r != sl::Result::eOk) {
        error = "this graphics card or its driver has no DLSS (" + std::to_string(int(r)) + ")";
        return false;
    }
    if (generation_) {
        r = slIsFeatureSupported(sl::kFeatureDLSS_G, adapter);
        if (r != sl::Result::eOk) {
            LOG_ERROR("DLSS frame generation is not available (%d): a GeForce RTX 40 or 50, a recent driver and "
                      "Windows' hardware-accelerated GPU scheduling are needed",
                      int(r));
        } else {
            sl::ReflexOptions reflex;
            reflex.mode = sl::ReflexMode::eLowLatency;  // frame generation works only with Reflex on
            const sl::Result rr = slReflexSetOptions(reflex);
            if (rr != sl::Result::eOk) LOG_ERROR("%s", result("slReflexSetOptions", rr).c_str());
            generationReady_ = true;
        }
    }
    return true;
}

bool Dlss::renderSize(int outW, int outH, int& renderW, int& renderH) {
    sl::DLSSOptions o;
    o.mode = sl::DLSSMode(mode_);
    o.outputWidth = uint32_t(outW);
    o.outputHeight = uint32_t(outH);
    sl::DLSSOptimalSettings s;
    const sl::Result r = slDLSSGetOptimalSettings(o, s);
    if (r != sl::Result::eOk || !s.optimalRenderWidth || !s.optimalRenderHeight) {
        failed(result("slDLSSGetOptimalSettings", r));
        return false;
    }
    renderW = int(s.optimalRenderWidth);
    renderH = int(s.optimalRenderHeight);
    return true;
}

bool Dlss::setOptions(int outW, int outH) {
    if (outW == optionsW_ && outH == optionsH_) return true;
    sl::DLSSOptions o;
    o.mode = sl::DLSSMode(mode_);
    o.outputWidth = uint32_t(outW);
    o.outputHeight = uint32_t(outH);
    o.colorBuffersHDR = sl::Boolean::eFalse;  // the game's colours, 0..1
    o.dlaaPreset = o.qualityPreset = o.balancedPreset = sl::DLSSPreset::ePresetK;  // the transformer model
    o.performancePreset = sl::DLSSPreset::ePresetM;
    o.ultraPerformancePreset = sl::DLSSPreset::ePresetL;
    const sl::Result r = slDLSSSetOptions(kViewport, o);
    if (r != sl::Result::eOk) {
        failed(result("slDLSSSetOptions", r));
        return false;
    }
    optionsW_ = outW;
    optionsH_ = outH;
    return true;
}

bool Dlss::evaluate(const Frame& f) {
    if (!started_ || !setOptions(f.outW, f.outH)) return false;
    if (!generationReady_) token_ = nullptr;  // without frame generation every upscale is a frame of its own
    sl::FrameToken* token = static_cast<sl::FrameToken*>(this->token());
    if (!token) return false;
    sl::Result r = sl::Result::eOk;
    sl::Constants c;
    std::memcpy(&c.cameraViewToClip, f.viewToClip, sizeof f.viewToClip);
    std::memcpy(&c.clipToCameraView, f.clipToView, sizeof f.clipToView);
    std::memcpy(&c.clipToPrevClip, f.clipToPrevClip, sizeof f.clipToPrevClip);
    std::memcpy(&c.prevClipToClip, f.prevClipToClip, sizeof f.prevClipToClip);
    const float identity[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    std::memcpy(&c.clipToLensClip, identity, sizeof identity);
    c.jitterOffset = {jitterSign_ * f.jitter[0], jitterSign_ * f.jitter[1]};
    c.mvecScale = {motionSign_ / float(f.renderW), motionSign_ / float(f.renderH)};  // render pixels
    c.cameraPinholeOffset = {0.0f, 0.0f};
    c.cameraPos = {f.cameraPos[0], f.cameraPos[1], f.cameraPos[2]};
    c.cameraUp = {f.cameraUp[0], f.cameraUp[1], f.cameraUp[2]};
    c.cameraRight = {f.cameraRight[0], f.cameraRight[1], f.cameraRight[2]};
    c.cameraFwd = {f.cameraFwd[0], f.cameraFwd[1], f.cameraFwd[2]};
    c.cameraNear = f.nearZ;
    c.cameraFar = f.farZ;
    c.cameraFOV = f.fovY;
    c.cameraAspectRatio = f.aspect;
    c.depthInverted = sl::Boolean::eFalse;
    c.cameraMotionIncluded = sl::Boolean::eTrue;
    c.motionVectors3D = sl::Boolean::eFalse;
    c.reset = f.reset ? sl::Boolean::eTrue : sl::Boolean::eFalse;
    c.orthographicProjection = sl::Boolean::eFalse;
    c.motionVectorsDilated = sl::Boolean::eFalse;
    c.motionVectorsJittered = sl::Boolean::eFalse;
    r = slSetConstants(c, *token, kViewport);
    if (r != sl::Result::eOk) {
        failed(result("slSetConstants", r));
        return false;
    }
    sl::Resource color(sl::ResourceType::eTex2d, f.color, uint32_t(f.colorState));
    sl::Resource output(sl::ResourceType::eTex2d, f.output, uint32_t(f.outputState));
    sl::Resource depth(sl::ResourceType::eTex2d, f.depth, uint32_t(f.depthState));
    sl::Resource motion(sl::ResourceType::eTex2d, f.motion, uint32_t(f.motionState));
    const sl::Extent in{0, 0, uint32_t(f.renderW), uint32_t(f.renderH)};
    // Frame generation reads depth and motion at the present: they stay as they are until then.
    const sl::ResourceLifecycle lasting =
        generationReady_ ? sl::ResourceLifecycle::eValidUntilPresent : sl::ResourceLifecycle::eOnlyValidNow;
    const sl::Extent out{0, 0, uint32_t(f.outW), uint32_t(f.outH)};
    const sl::ResourceTag tags[4] = {
        sl::ResourceTag(&color, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eOnlyValidNow, &in),
        sl::ResourceTag(&output, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eOnlyValidNow, &out),
        sl::ResourceTag(&depth, sl::kBufferTypeDepth, lasting, &in),
        sl::ResourceTag(&motion, sl::kBufferTypeMotionVectors, lasting, &in)};
    auto* list = reinterpret_cast<sl::CommandBuffer*>(f.list);
    r = slSetTagForFrame(*token, kViewport, tags, 4, list);
    if (r != sl::Result::eOk) {
        failed(result("slSetTagForFrame", r));
        return false;
    }
    const sl::BaseStructure* inputs[1] = {&kViewport};
    r = slEvaluateFeature(sl::kFeatureDLSS, *token, inputs, 1, list);
    if (r != sl::Result::eOk) {
        failed(result("slEvaluateFeature", r));
        return false;
    }
    return true;
}

void* Dlss::token() {
    if (!token_) {
        sl::FrameToken* t = nullptr;
        const uint32_t index = frame_++;
        if (slGetNewFrameToken(t, &index) == sl::Result::eOk) token_ = t;
    }
    return token_;
}

void Dlss::marker(uint32_t m) {
    if (!generationReady_ || !token()) return;
    slPCLSetMarker(sl::PCLMarker(m), *static_cast<sl::FrameToken*>(token_));
}

void Dlss::renderStart() {
    marker(uint32_t(sl::PCLMarker::eSimulationEnd));  // the game has worked out its frame: its draws arrive
    marker(uint32_t(sl::PCLMarker::eRenderSubmitStart));
}

void Dlss::beforePresent(const Generation& g) {
    if (!generationReady_ || !token()) return;
    auto* frame = static_cast<sl::FrameToken*>(token_);
    auto* list = reinterpret_cast<sl::CommandBuffer*>(g.list);
    const sl::Extent picture{uint32_t(g.picture.top), uint32_t(g.picture.left),
                             uint32_t(g.picture.right - g.picture.left), uint32_t(g.picture.bottom - g.picture.top)};
    sl::Resource hudless(sl::ResourceType::eTex2d, g.hudless, uint32_t(g.hudlessState));
    sl::Resource ui(sl::ResourceType::eTex2d, g.ui, uint32_t(g.uiState));
    sl::Resource back(sl::ResourceType::eTex2d, nullptr, 0u);  // the swap chain's: only its part with the picture
    std::vector<sl::ResourceTag> tags = {
        sl::ResourceTag(&hudless, sl::kBufferTypeHUDLessColor, sl::ResourceLifecycle::eValidUntilPresent, &picture),
        sl::ResourceTag(&ui, sl::kBufferTypeUIColorAndAlpha, sl::ResourceLifecycle::eValidUntilPresent, &picture)};
    if (picture.width != uint32_t(g.backWidth) || picture.height != uint32_t(g.backHeight))
        tags.push_back(sl::ResourceTag(&back, sl::kBufferTypeBackbuffer, sl::ResourceLifecycle::eValidUntilPresent,
                                       &picture));
    sl::Result r = slSetTagForFrame(*frame, kViewport, tags.data(), uint32_t(tags.size()), list);
    if (r != sl::Result::eOk) failed(result("slSetTagForFrame (frame generation)", r));
    const int mode = g.on ? 1 : 0;
    if (mode != generationMode_) {
        if (mode && generationMode_ < 0) {  // the first time: as many frames as this card can make
            sl::DLSSGState state;
            if (slDLSSGGetState(kViewport, state, nullptr) == sl::Result::eOk && state.numFramesToGenerateMax > 0) {
                generationFrames_ = std::min<uint32_t>(uint32_t(generation_ - 1), state.numFramesToGenerateMax);
                if (generationFrames_ + 1 < uint32_t(generation_))
                    LOG_INFO("DLSS frame generation: %ux on this card instead of %dx", generationFrames_ + 1,
                             generation_);
            } else {
                generationFrames_ = 1;
            }
        }
        sl::DLSSGOptions o;
        o.mode = mode ? sl::DLSSGMode::eOn : sl::DLSSGMode::eOff;
        o.numFramesToGenerate = generationFrames_;
        o.flags = sl::DLSSGFlags::eRetainResourcesWhenOff;  // menus come and go: no stutter when it comes back
        r = slDLSSGSetOptions(kViewport, o);
        if (r != sl::Result::eOk) {
            failed(result("slDLSSGSetOptions", r));
        } else {
            LOG_INFO("DLSS frame generation %s", mode ? "on" : "off (no world on screen)");
            generationMode_ = mode;
        }
    }
    marker(uint32_t(sl::PCLMarker::eRenderSubmitEnd));
    marker(uint32_t(sl::PCLMarker::ePresentStart));
    presentStarted_ = true;
}

void Dlss::suspend() {
    if (!generationReady_ || generationMode_ != 1) return;
    sl::DLSSGOptions o;
    o.mode = sl::DLSSGMode::eOff;
    o.numFramesToGenerate = generationFrames_;
    o.flags = sl::DLSSGFlags::eRetainResourcesWhenOff;
    if (slDLSSGSetOptions(kViewport, o) == sl::Result::eOk) generationMode_ = 0;
}

void Dlss::afterPresent() {
    if (!generationReady_) return;
    if (presentStarted_) marker(uint32_t(sl::PCLMarker::ePresentEnd));
    presentStarted_ = false;
    // The next frame: its own token, Reflex's sleep, then its simulation. Every frame, also one whose upscale failed
    // or that had nothing to generate around: a token kept on would take the next frame's constants as a second set
    // for the same frame (0.9: "Setting different 'common' constants multiple times within the same frame").
    token_ = nullptr;
    if (token()) {
        slReflexSleep(*static_cast<sl::FrameToken*>(token_));
        marker(uint32_t(sl::PCLMarker::eSimulationStart));
    }
}

}  // namespace uagfx::host
