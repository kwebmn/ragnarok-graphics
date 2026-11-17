// 3.0.21-beta.14 (R-DP7PVN: the game gone a second after «Довідник»'s card of Prontera opened, RmlUi's «Could not
// load texture: pic:1dc49ab8:1937876632» twice before it - a picture's source was read back as its address frames
// after it was named, the picture gone by then): the pictures named for RmlUi kept by their sources (core/ui_sources)
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/ui_sources.hpp"
#include "core/window_panel.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {
panel::Picture square(int side, uint32_t colour) {
    panel::Picture p;
    p.width = p.height = side;
    p.px.assign(size_t(side) * size_t(side), colour);
    return p;
}
bool listed(const std::vector<std::string>& list, const std::string& s) {
    return std::find(list.begin(), list.end(), s) != list.end();
}
}  // namespace

TEST(ui_sources_a_picture_loads_after_it_is_gone) {
    uisources::Sources sources;
    std::string slot;
    std::vector<std::string> release;
    auto p = std::make_unique<panel::Picture>(square(4, 0xFF112233u));
    const std::string src = sources.name(p.get(), 7, slot, release);
    p.reset();  // its maker let it go; RmlUi asks for it frames later (when the row comes into view)
    const uisources::Kept* k = sources.find(src);
    CHECK(k != nullptr);
    CHECK(k && k->px);
    CHECK_EQ(k ? k->width : 0, 4);
    CHECK_EQ(k ? k->height : 0, 4);
    CHECK_EQ(k && k->px ? k->px->size() : size_t(0), size_t(16));
    CHECK(k && k->px && (*k->px)[15] == 0xFF112233u);
}

TEST(ui_sources_a_source_never_named_does_not_load) {
    uisources::Sources sources;
    panel::Picture p = square(2, 1);
    char src[48];
    std::snprintf(src, sizeof src, "pic:%llx:%d", (unsigned long long)uintptr_t(&p), 3);
    CHECK(sources.find(src) == nullptr);  // an address in a source is never read back
    CHECK(sources.find("pic:1dc49ab8:1937876632") == nullptr);
    CHECK(sources.find("icon:1dc49ab8") == nullptr);
}

TEST(ui_sources_a_shared_source_goes_with_its_last_place) {
    uisources::Sources sources;
    panel::Picture a = square(2, 1), b = square(2, 2);
    std::string x, y;
    std::vector<std::string> release;
    const std::string sa = sources.name(&a, 1, x, release);
    CHECK_EQ(sources.name(&a, 1, y, release), sa);  // two places (the hotbar, the items window), one icon
    release.clear();
    sources.name(&b, 1, x, release);  // the first place shows another picture
    CHECK(!listed(release, sa));      // the other still shows it: RmlUi keeps its texture
    CHECK(sources.find(sa) != nullptr);
    sources.name(nullptr, 0, y, release);  // the last place lets it go
    CHECK(listed(release, sa));
    CHECK(sources.find(sa) == nullptr);
    CHECK(y.empty());
}

TEST(ui_sources_another_generation_is_another_copy) {
    uisources::Sources sources;
    panel::Picture p = square(2, 0xFF000001u);
    std::string slot;
    std::vector<std::string> release;
    const std::string first = sources.name(&p, 1, slot, release);
    p.px.assign(4, 0xFF000002u);  // drawn again in place
    release.clear();
    const std::string second = sources.name(&p, 2, slot, release);
    CHECK(first != second);
    CHECK(listed(release, first));
    CHECK(sources.find(first) == nullptr);
    const uisources::Kept* k = sources.find(second);
    CHECK(k && k->px && (*k->px)[0] == 0xFF000002u);
}

TEST(ui_sources_a_source_named_again_is_told_to_rmlui) {
    // forgotten, then named again (a place back to its old picture): RmlUi's failed load of it in between must not
    // stay - the source is listed (win/uaui: Rml::ReleaseTexture, which clears a failed load - third_party/rmlui)
    uisources::Sources sources;
    panel::Picture a = square(2, 1), b = square(2, 2);
    std::string slot;
    std::vector<std::string> release;
    const std::string sa = sources.name(&a, 1, slot, release);
    sources.name(&b, 1, slot, release);
    release.clear();
    CHECK_EQ(sources.name(&a, 1, slot, release), sa);
    CHECK(listed(release, sa));
    CHECK(sources.find(sa) != nullptr);
}

TEST(ui_sources_the_same_source_again_changes_nothing) {
    uisources::Sources sources;
    panel::Picture a = square(2, 1);
    std::string slot;
    std::vector<std::string> release;
    const std::string sa = sources.name(&a, 1, slot, release);
    release.clear();
    CHECK_EQ(sources.name(&a, 1, slot, release), sa);  // every frame the part asks
    CHECK(release.empty());
    CHECK_EQ(sources.size(), size_t(1));
    sources.name(nullptr, 0, slot, release);
    CHECK_EQ(sources.size(), size_t(0));  // its one place gone: nothing kept
}

TEST(ui_sources_a_picture_without_its_pixels_names_a_source_that_does_not_load) {
    uisources::Sources sources;
    panel::Picture p;
    p.width = p.height = 3;  // a size, no pixels yet
    std::string slot;
    std::vector<std::string> release;
    const std::string src = sources.name(&p, 1, slot, release);
    CHECK(!src.empty());
    const uisources::Kept* k = sources.find(src);
    CHECK(k == nullptr || !k->px);
}
