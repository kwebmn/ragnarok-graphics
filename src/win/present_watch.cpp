#include "win/present_watch.hpp"

#include <dxgi1_6.h>

#include <cstdint>
#include <string>

#include "core/gpu_timing.hpp"
#include "core/present_watch.hpp"
#include "core/settings/strings.hpp"
#include "win/code_page.hpp"
#include "win/game_camera.hpp"
#include "win/gpu/renderer.hpp"
#include "win/log.hpp"
#include "win/real_ddraw.hpp"
#include "win/setting_edit.hpp"

namespace uagfx::win::presentwatchwin {
namespace {

constexpr uint32_t kWindowMgr = 0x727ED8, kPush = 0x4DB310;  // the chat's line (win/console_hint.cpp)
constexpr int kLineColour = 0x64FFFF;
using PushFn = int(__attribute__((thiscall)) *)(uint32_t manager, int msg, const char* text, int colour, int more);

struct State {
    presentwatch::Watch watch;
    uint32_t serial = 0;
    bool cardsTold = false;
    std::string screen;  // the monitor's card when another than the one drawing
};

State& state() {
    static State s;
    return s;
}

std::string narrowName(const wchar_t* w) {
    char out[256] = {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, out, sizeof out, nullptr, nullptr);
    return n > 0 ? std::string(out) : std::string();
}

void tellInChat(const std::string& utf8, unsigned codePage) {
    if (!inGameWorld()) return;
    const std::string text = narrowed(widened(utf8, CP_UTF8), codePage);
    reinterpret_cast<PushFn>(uintptr_t(kPush))(kWindowMgr, 1, text.c_str(), kLineColour, 0);
}

}  // namespace

std::string screenCard(HWND game) {
    HMODULE dxgi = GetModuleHandleA("dxgi.dll");
    if (!dxgi) dxgi = LoadLibraryA("dxgi.dll");
    using CreateFn = HRESULT(WINAPI*)(REFIID, void**);
    void* const entry = dxgi ? reinterpret_cast<void*>(GetProcAddress(dxgi, "CreateDXGIFactory1")) : nullptr;
    const auto create = reinterpret_cast<CreateFn>(entry);
    IDXGIFactory1* factory = nullptr;
    if (!create || FAILED(create(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) || !factory) return {};
    const HMONITOR monitor = MonitorFromWindow(game, MONITOR_DEFAULTTONEAREST);
    std::string found;
    IDXGIAdapter1* adapter = nullptr;
    for (UINT i = 0; found.empty() && factory->EnumAdapters1(i, &adapter) == S_OK; ++i) {
        IDXGIOutput* output = nullptr;
        for (UINT j = 0; found.empty() && adapter->EnumOutputs(j, &output) == S_OK; ++j) {
            DXGI_OUTPUT_DESC d{};
            if (SUCCEEDED(output->GetDesc(&d)) && d.Monitor == monitor) {
                DXGI_ADAPTER_DESC1 a{};
                if (SUCCEEDED(adapter->GetDesc1(&a))) found = narrowName(a.Description);
            }
            output->Release();
        }
        adapter->Release();
    }
    factory->Release();
    return found;
}

void frame(gpu::Renderer& renderer, const Config& config, const std::string& iniPath, HWND game, unsigned codePage) {
    State& s = state();
    if (!inGameWorld() || !game || GetForegroundWindow() != game) {
        s.watch.reset();
        return;
    }
    if (!s.cardsTold) {  // once, in the world: which card draws, which one the screen hangs off
        s.cardsTold = true;
        const std::string drawing = renderer.adapterName(), screen = screenCard(game);
        if (!screen.empty() && !drawing.empty() && screen != drawing) {
            s.screen = screen;
            LOG_INFO("graphics cards: the game's monitor hangs off %s, the frames are drawn on %s - each one crosses "
                     "between the cards", screen.c_str(), drawing.c_str());
        } else {
            LOG_INFO("graphics cards: the game's monitor hangs off %s (drawn on %s)",
                     screen.empty() ? "a card DXGI does not name" : screen.c_str(), drawing.c_str());
        }
    }
    GpuTiming t;
    if (!renderer.gpuTiming(t) || t.serial == s.serial) return;
    s.serial = t.serial;
    if (t.loading || t.presentMs <= 0.0) return;
    presentwatch::Sample sample;
    sample.presentMs = t.presentMs;
    sample.gpuMs = t.gpuMs;
    sample.cpuMs = t.cpuMs;
    const bool d3d12 = !renderer.d3d11();
    const bool before = s.watch.taken();
    const presentwatch::Step step = s.watch.feed(sample, d3d12, config.crossPresent, s.screen);
    const std::string ms = std::to_string(int(t.presentMs + 0.5));
    if (step == presentwatch::Step::ToCrossPresent) {  // 3.0.19-beta.9: at once - the renderer takes it at a present
        settingedit::set(iniPath, config, "renderer.cross_present", "1");
        LOG_INFO("present watch: 10 s of slow presents (%.0f ms each, the card's work %.1f ms) - the frames through %s "
                 "from now ([renderer] cross_present = 1)", t.presentMs, t.gpuMs, s.screen.c_str());
        tellInChat(settings::fmt(settings::tr(language(), settings::Text::PresentSlowCross),
                                 {ms, renderer.adapterName()}),
                   codePage);
    } else if (step == presentwatch::Step::ToD3d11) {
        settingedit::set(iniPath, config, "renderer.api", "d3d11");
        LOG_INFO("present watch: 10 s of slow presents (%.0f ms each, the card's work %.1f ms) - Direct3D 11 from the "
                 "next start ([renderer] api = d3d11)", t.presentMs, t.gpuMs);
        tellInChat(settings::fmt(settings::tr(language(), settings::Text::PresentSlowD3d11), {ms}), codePage);
    } else if (step == presentwatch::Step::ToScreenCard) {
        settingedit::set(iniPath, config, "renderer.gpu", s.screen);
        LOG_INFO("present watch: 10 s of slow presents on Direct3D 11 (%.0f ms each) - the game drawn on %s from the "
                 "next start ([renderer] gpu)", t.presentMs, s.screen.c_str());
        tellInChat(settings::fmt(settings::tr(language(), settings::Text::PresentSlowCard), {ms, s.screen}),
                   codePage);
    } else if (!before && s.watch.taken()) {
        LOG_INFO("present watch: 10 s of slow presents (%.0f ms each, the card's work %.1f ms) - nothing to turn to "
                 "(Direct3D 11, the screen on the card drawing)", t.presentMs, t.gpuMs);
    }
}

}  // namespace uagfx::win::presentwatchwin
