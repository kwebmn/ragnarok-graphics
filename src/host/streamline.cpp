#include "host/streamline.hpp"

#include <cstring>
#include <vector>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include "sl.h"
#include "sl_dlss.h"
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

void slLog(sl::LogType type, const char* text) {
    if (type == sl::LogType::eInfo) return;
    std::string line = text ? text : "";
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
    LOG_INFO("streamline: %s", line.c_str());
}

std::wstring wide(const std::string& s) {
    std::wstring w(s.size() + 1, L'\0');
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), int(s.size()), &w[0], int(w.size()));
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

bool Dlss::init(const std::string& folder, const std::string& logFolder, const std::string& mode, std::string& error) {
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
    sl::Feature features[1] = {sl::kFeatureDLSS};
    sl::Preferences pref{};
    pref.showConsole = false;
    pref.logLevel = sl::LogLevel::eDefault;
    pref.pathsToPlugins = paths;
    pref.numPathsToPlugins = 1;
    pref.pathToLogsAndData = logs.c_str();
    pref.logMessageCallback = slLog;
    pref.flags = sl::PreferenceFlags::eDisableCLStateTracking;  // the renderer sets its state again itself; no OTA
    pref.featuresToLoad = features;
    pref.numFeaturesToLoad = 1;
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
        LOG_ERROR("%s", result("slDLSSGetOptimalSettings", r).c_str());
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
        LOG_ERROR("%s", result("slDLSSSetOptions", r).c_str());
        return false;
    }
    optionsW_ = outW;
    optionsH_ = outH;
    return true;
}

bool Dlss::evaluate(const Frame& f) {
    if (!started_ || !setOptions(f.outW, f.outH)) return false;
    sl::FrameToken* token = nullptr;
    const uint32_t index = frame_++;
    sl::Result r = slGetNewFrameToken(token, &index);
    if (r != sl::Result::eOk || !token) return false;
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
        LOG_ERROR("%s", result("slSetConstants", r).c_str());
        return false;
    }
    sl::Resource color(sl::ResourceType::eTex2d, f.color, uint32_t(f.colorState));
    sl::Resource output(sl::ResourceType::eTex2d, f.output, uint32_t(f.outputState));
    sl::Resource depth(sl::ResourceType::eTex2d, f.depth, uint32_t(f.depthState));
    sl::Resource motion(sl::ResourceType::eTex2d, f.motion, uint32_t(f.motionState));
    const sl::Extent in{0, 0, uint32_t(f.renderW), uint32_t(f.renderH)};
    const sl::Extent out{0, 0, uint32_t(f.outW), uint32_t(f.outH)};
    const sl::ResourceTag tags[4] = {
        sl::ResourceTag(&color, sl::kBufferTypeScalingInputColor, sl::ResourceLifecycle::eOnlyValidNow, &in),
        sl::ResourceTag(&output, sl::kBufferTypeScalingOutputColor, sl::ResourceLifecycle::eOnlyValidNow, &out),
        sl::ResourceTag(&depth, sl::kBufferTypeDepth, sl::ResourceLifecycle::eOnlyValidNow, &in),
        sl::ResourceTag(&motion, sl::kBufferTypeMotionVectors, sl::ResourceLifecycle::eOnlyValidNow, &in)};
    auto* list = reinterpret_cast<sl::CommandBuffer*>(f.list);
    r = slSetTagForFrame(*token, kViewport, tags, 4, list);
    if (r != sl::Result::eOk) {
        LOG_ERROR("%s", result("slSetTagForFrame", r).c_str());
        return false;
    }
    const sl::BaseStructure* inputs[1] = {&kViewport};
    r = slEvaluateFeature(sl::kFeatureDLSS, *token, inputs, 1, list);
    if (r != sl::Result::eOk) {
        LOG_ERROR("%s", result("slEvaluateFeature", r).c_str());
        return false;
    }
    return true;
}

}  // namespace uagfx::host
