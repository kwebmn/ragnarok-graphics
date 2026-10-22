#include <cstring>
#include <string>

#include "core/renderer_choice.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
Config wants() { return Config(); }
SystemFacts everything() {
    SystemFacts f;
    f.windows10 = true;
    f.d3d12 = true;
    return f;
}
bool mentions(const std::string& reason, const char* what) { return reason.find(what) != std::string::npos; }
}  // namespace

TEST(choose_uaro_when_everything_is_there) {
    const RendererChoice r = chooseRenderer(wants(), everything(), true);
    CHECK(r.renderer == Renderer::Uaro);
}

TEST(choose_uaro_not_built_falls_back_to_system) {
    const RendererChoice r = chooseRenderer(wants(), everything(), false);
    CHECK(r.renderer == Renderer::System);
    CHECK(mentions(r.reason, "not in this version"));
}

TEST(choose_uaro_needs_windows10) {
    SystemFacts f = everything();
    f.windows10 = false;
    const RendererChoice r = chooseRenderer(wants(), f, true);
    CHECK(r.renderer == Renderer::System);
    CHECK(mentions(r.reason, "Windows 10"));
}

TEST(choose_uaro_needs_d3d12) {
    SystemFacts f = everything();
    f.d3d12 = false;
    const RendererChoice r = chooseRenderer(wants(), f, true);
    CHECK(r.renderer == Renderer::System);
    CHECK(mentions(r.reason, "Direct3D 12"));
}

TEST(choose_uaro_failure_goes_to_system) {
    SystemFacts f;  // nothing at all: old Windows, no D3D12
    const RendererChoice r = chooseRenderer(wants(), f, true);
    CHECK(r.renderer == Renderer::System);
    CHECK(mentions(r.reason, "Windows 10"));
}

TEST(renderer_names) {
    CHECK(std::strcmp(rendererName(Renderer::Uaro), "uaRO Graphics") == 0);
    CHECK(std::strcmp(rendererName(Renderer::System), "system DirectDraw") == 0);
}

// 0.46: [renderer] api - Direct3D 12 (as before), Direct3D 11 (graphics cards without Direct3D 12) or auto (12, else
// 11). Direct3D 11 needs no Windows 10 - a device of feature level 10_0 or better is enough.
namespace {
Config wantsApi(Api api) {
    Config c = wants();
    c.api = api;
    return c;
}
}  // namespace

TEST(choose_direct3d_12_by_default) {
    SystemFacts f = everything();
    f.d3d11 = true;
    const RendererChoice r = chooseRenderer(wants(), f, true);
    CHECK(r.renderer == Renderer::Uaro && r.api == Api::D3d12);
}

TEST(choose_direct3d_11_when_asked) {
    SystemFacts f = everything();
    f.d3d11 = true;
    const RendererChoice r = chooseRenderer(wantsApi(Api::D3d11), f, true);
    CHECK(r.renderer == Renderer::Uaro && r.api == Api::D3d11);
    CHECK(mentions(r.reason, "api = d3d11"));
    // Before Windows 10 too.
    f.windows10 = false;
    f.d3d12 = false;
    const RendererChoice old = chooseRenderer(wantsApi(Api::D3d11), f, true);
    CHECK(old.renderer == Renderer::Uaro && old.api == Api::D3d11);
}

TEST(choose_direct3d_11_needs_its_device) {
    SystemFacts f = everything();  // no Direct3D 11 device
    const RendererChoice r = chooseRenderer(wantsApi(Api::D3d11), f, true);
    CHECK(r.renderer == Renderer::System);
    CHECK(mentions(r.reason, "Direct3D 11"));
}

TEST(choose_auto_takes_direct3d_12_then_11) {
    SystemFacts f = everything();
    f.d3d11 = true;
    RendererChoice r = chooseRenderer(wantsApi(Api::Auto), f, true);
    CHECK(r.renderer == Renderer::Uaro && r.api == Api::D3d12);
    f.d3d12 = false;
    r = chooseRenderer(wantsApi(Api::Auto), f, true);
    CHECK(r.renderer == Renderer::Uaro && r.api == Api::D3d11);
    CHECK(mentions(r.reason, "no Direct3D 12"));
    f.windows10 = false;
    r = chooseRenderer(wantsApi(Api::Auto), f, true);
    CHECK(r.renderer == Renderer::Uaro && r.api == Api::D3d11);
    f.d3d11 = false;
    r = chooseRenderer(wantsApi(Api::Auto), f, true);
    CHECK(r.renderer == Renderer::System);
    CHECK(mentions(r.reason, "Direct3D 11"));
}

// 2.12: «Вимкнути uaRO Graphics» - api = system: the game's own look (the pad, the sound and the launcher stay)
TEST(api_system_is_the_games_own_look) {
    Config c;
    c.api = Api::System;
    const RendererChoice r = chooseRenderer(c, everything(), true);
    CHECK(r.renderer == Renderer::System);
    CHECK(mentions(r.reason, "api = system"));
    CHECK(parseConfig("[renderer]\napi = system\n").api == Api::System);
    CHECK(std::strcmp(apiToken(Api::System), "system") == 0);
}
