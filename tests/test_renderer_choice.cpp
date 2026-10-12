#include <cstring>
#include <string>

#include "core/renderer_choice.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
Config wants(Backend b) {
    Config c;
    c.backend = b;
    return c;
}
SystemFacts everything() {
    SystemFacts f;
    f.windows10 = true;
    f.d3d12 = true;
    f.dgvoodoo = true;
    return f;
}
bool mentions(const std::string& reason, const char* what) { return reason.find(what) != std::string::npos; }
}  // namespace

TEST(choose_uaro_when_everything_is_there) {
    const RendererChoice r = chooseRenderer(wants(Backend::Uaro), everything(), true);
    CHECK(r.renderer == Renderer::Uaro);
}

TEST(choose_dgvoodoo_when_asked) {
    const RendererChoice r = chooseRenderer(wants(Backend::DgVoodoo), everything(), true);
    CHECK(r.renderer == Renderer::DgVoodoo);
    CHECK(mentions(r.reason, "backend = dgvoodoo"));
}

TEST(choose_uaro_not_built_falls_back_to_dgvoodoo) {
    const RendererChoice r = chooseRenderer(wants(Backend::Uaro), everything(), false);
    CHECK(r.renderer == Renderer::DgVoodoo);
    CHECK(mentions(r.reason, "not in this version"));
}

TEST(choose_uaro_needs_windows10) {
    SystemFacts f = everything();
    f.windows10 = false;
    const RendererChoice r = chooseRenderer(wants(Backend::Uaro), f, true);
    CHECK(r.renderer == Renderer::DgVoodoo);
    CHECK(mentions(r.reason, "Windows 10"));
}

TEST(choose_uaro_needs_d3d12) {
    SystemFacts f = everything();
    f.d3d12 = false;
    const RendererChoice r = chooseRenderer(wants(Backend::Uaro), f, true);
    CHECK(r.renderer == Renderer::DgVoodoo);
    CHECK(mentions(r.reason, "Direct3D 12"));
}

TEST(choose_falls_back_to_system_without_dgvoodoo) {
    SystemFacts f = everything();
    f.dgvoodoo = false;
    const RendererChoice r = chooseRenderer(wants(Backend::DgVoodoo), f, true);
    CHECK(r.renderer == Renderer::System);
    CHECK(mentions(r.reason, "dgvoodoo\\DDraw.dll not found"));
}

TEST(choose_uaro_failure_without_dgvoodoo_goes_to_system) {
    SystemFacts f;  // nothing at all: old Windows, no D3D12, no dgVoodoo
    const RendererChoice r = chooseRenderer(wants(Backend::Uaro), f, true);
    CHECK(r.renderer == Renderer::System);
    CHECK(mentions(r.reason, "Windows 10"));
}

TEST(renderer_names) {
    CHECK(std::strcmp(rendererName(Renderer::Uaro), "uaRO Graphics") == 0);
    CHECK(std::strcmp(rendererName(Renderer::DgVoodoo), "dgVoodoo") == 0);
    CHECK(std::strcmp(rendererName(Renderer::System), "system DirectDraw") == 0);
}
