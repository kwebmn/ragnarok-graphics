#include <cstdint>
#include <vector>

#include "core/shader_cache.hpp"
#include "test.hpp"

using namespace uagfx;
using shader_cache::Shader;

namespace {

std::vector<Shader> twoShaders() {
    return {{"draw_ps", "ps_5_0", {0x44, 0x58, 0x42, 0x43, 1, 2, 3, 4, 5}}, {"present_vs", "vs_5_0", {9, 8, 7}}};
}

bool same(const std::vector<Shader>& a, const std::vector<Shader>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i].entry != b[i].entry || a[i].profile != b[i].profile || a[i].code != b[i].code) return false;
    return true;
}

}  // namespace

TEST(shader_cache_gives_back_the_code_it_kept) {
    // 0.39.5: the code of each entry point and profile, as it was kept, for the same source.
    const uint64_t key = shader_cache::keyOf("float4 main() : SV_Target { return 1; }");
    const std::vector<uint8_t> file = shader_cache::pack(key, twoShaders());
    CHECK(!file.empty());
    CHECK(same(shader_cache::unpack(key, file), twoShaders()));
    CHECK(shader_cache::unpack(key, shader_cache::pack(key, {})).empty());  // nothing kept: nothing given
}

TEST(shader_cache_knows_the_source_it_was_made_for) {
    // Another version's shaders (another source) are compiled anew: nothing of the file is used.
    const uint64_t a = shader_cache::keyOf("float4 main() : SV_Target { return 1; }");
    const uint64_t b = shader_cache::keyOf("float4 main() : SV_Target { return 0; }");
    CHECK(a != b);
    CHECK(a == shader_cache::keyOf("float4 main() : SV_Target { return 1; }"));
    CHECK(shader_cache::unpack(b, shader_cache::pack(a, twoShaders())).empty());
}

TEST(shader_cache_uses_nothing_of_a_damaged_file) {
    // A file cut short (the game closed while it was written) or with a byte changed gives nothing - never a part.
    const uint64_t key = shader_cache::keyOf("source");
    const std::vector<uint8_t> file = shader_cache::pack(key, twoShaders());
    for (size_t n = 0; n < file.size(); ++n) {
        const std::vector<uint8_t> cut(file.begin(), file.begin() + long(n));
        CHECK(shader_cache::unpack(key, cut).empty());
    }
    for (size_t i = 0; i < file.size(); ++i) {
        std::vector<uint8_t> changed = file;
        changed[i] ^= 0x20;
        CHECK(shader_cache::unpack(key, changed).empty());
    }
    std::vector<uint8_t> longer = file;
    longer.push_back(0);  // something after the end: not this file
    CHECK(shader_cache::unpack(key, longer).empty());
    CHECK(shader_cache::unpack(key, {}).empty());
}

TEST(shader_cache_keeps_whether_a_shader_lost_its_newest_features) {
    // 2.2.2: a shader compiled with UAGFX_SAFE is kept so - the next starts know it too (the statistics count them)
    const uagfx::shader_cache::Shader plain{"draw_ps", uagfx::shader_cache::profileOf("ps_5_0", false), {1}};
    const uagfx::shader_cache::Shader safe{"draw_ps", uagfx::shader_cache::profileOf("ps_5_0", true), {2}};
    bool lost = true;
    CHECK(uagfx::shader_cache::matches(plain, "draw_ps", "ps_5_0", lost) && !lost);
    CHECK(uagfx::shader_cache::matches(safe, "draw_ps", "ps_5_0", lost) && lost);
    CHECK(!uagfx::shader_cache::matches(safe, "draw_ps", "vs_5_0", lost));
    CHECK(!uagfx::shader_cache::matches(plain, "glow_ps", "ps_5_0", lost));
}

// 3.0.6-beta.3: ray_glow_ps's optimiser level 3 fails inside FXC on every computer seen (R-7JQ6XX's RTX 3060, R-ZSDWYU's
// FirePro, the user's RTX 5060 Ti and Radeon 610M: «argument pulled into unrelated predicate») and level 1, tried next,
// made it each time - begun there: the same code, no failed attempt at a first start; the others from level 3
TEST(shader_cache_begins_the_ray_glow_at_level_one) {
    CHECK_EQ(shader_cache::firstTry("ray_glow_ps"), 1);
    CHECK_EQ(shader_cache::firstTry("ray_ps"), 0);
    CHECK_EQ(shader_cache::firstTry("draw_ps"), 0);
}

// 3.0.24-beta.36 (the laptop's run, AMD Radeon 610M): the 64-bit renderer compiled its 84 shaders past the 90 s the
// game waits for it, was ended, kept nothing and began again at each start - the code of the other renderer's file
// (the game's own process: shaders-32.bin; the same source and key) taken too, its own first; and a start cut short
// keeps what it made (this start's first, then the last file's that it did not reach)
TEST(shader_cache_merged_takes_the_first_then_the_rest) {
    const std::vector<Shader> own = {{"draw_ps", "ps_5_0", {1}}, {"ray_glow_ps", "ps_5_0+safe", {2}}};
    const std::vector<Shader> other = {
        {"draw_ps", "ps_5_0", {9}}, {"ray_glow_ps", "ps_5_0", {8}}, {"taa_ps", "ps_5_0", {3}}, {"draw_vs", "vs_5_0", {4}}};
    const std::vector<Shader> m = shader_cache::merged(own, other);
    CHECK_EQ(m.size(), size_t(4));
    if (m.size() == 4) {
        CHECK(m[0].entry == "draw_ps" && m[0].code == std::vector<uint8_t>{1});
        CHECK(m[1].entry == "ray_glow_ps" && m[1].code == std::vector<uint8_t>{2});  // its own, though "+safe"
        CHECK(m[2].entry == "taa_ps" && m[3].entry == "draw_vs");                   // the rest in their order
    }
    CHECK_EQ(shader_cache::merged({}, other).size(), size_t(4));
    CHECK_EQ(shader_cache::merged(own, {}).size(), size_t(2));
    // an entry kept for two profiles is two shaders
    const std::vector<Shader> two = shader_cache::merged({{"x", "vs_5_0", {1}}}, {{"x", "ps_5_0", {2}}});
    CHECK_EQ(two.size(), size_t(2));
    // written and read back whole
    const uint64_t key = shader_cache::keyOf("source");
    CHECK_EQ(shader_cache::unpack(key, shader_cache::pack(key, m)).size(), size_t(4));
}
