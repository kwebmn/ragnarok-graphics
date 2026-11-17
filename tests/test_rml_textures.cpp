// 3.0.21-beta.14 (R-DP7PVN): RmlUi kept a texture's failed load for good - a picture's source named again
// (core/ui_sources: forgotten, then back) never showed. Rml::ReleaseTexture now clears a failed load too
// (third_party/rmlui/Source/Core/TextureDatabase.cpp, ours), so the source loads again when it is next needed.
#include <string>

#include "rml_host.hpp"
#include "test.hpp"

TEST(rml_a_failed_texture_loads_again_after_its_release) {
    const std::string src = "pic:f00d:1";
    rmlhost::failTextures({src});
    Rml::ElementDocument* doc = rmlhost::load(
        "<rml><head><link type=\"text/rcss\" href=\"uaui:theme.rcss\"/></head><body>"
        "<img src=\"" + src + "\" style=\"width: 10px; height: 10px;\"/></body></rml>",
        nullptr);
    CHECK(doc != nullptr);
    rmlhost::frame();
    CHECK_EQ(rmlhost::textureLoads(src), 1);
    rmlhost::frame();
    CHECK_EQ(rmlhost::textureLoads(src), 1);  // a failed load not asked again every frame
    rmlhost::failTextures({});
    Rml::ReleaseTexture(src);
    rmlhost::frame();
    CHECK_EQ(rmlhost::textureLoads(src), 2);
    rmlhost::close(doc);
}
