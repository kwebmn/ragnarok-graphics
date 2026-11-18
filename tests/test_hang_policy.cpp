#include <cstdint>
#include <vector>

#include "core/bridge_protocol.hpp"
#include "core/hang_policy.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.22-beta.8 (R-JW5W4N: the 64-bit renderer gone with 0xC0000409 after 2.2 s in one CreateTexture, two seconds
// into prontera; a fast fail leaves no dump): a texture's creation or release never takes a second - told from one
// second, and the renderer writes its own minidump at that first tell, once a game; the other messages (a frame's
// draws at a map, the pipelines made at the start) from two seconds, no dump
TEST(hang_policy_a_texture_told_at_a_second_with_a_dump) {
    using bridge::Msg;
    const std::vector<uint64_t> texture = {1000, 2000, 6000, 20000}, other = {2000, 6000, 20000};
    CHECK(hangpolicy::tellsFor(uint32_t(Msg::CreateTexture)) == texture);
    CHECK(hangpolicy::tellsFor(uint32_t(Msg::ReleaseTexture)) == texture);
    CHECK(hangpolicy::tellsFor(uint32_t(Msg::Execute)) == other);
    CHECK(hangpolicy::tellsFor(uint32_t(Msg::PreparePipelines)) == other);
    CHECK(hangpolicy::tellsFor(0) == other);  // the look at DLSS's start in the background
    CHECK(hangpolicy::dumpAt(uint32_t(Msg::CreateTexture), 0, false));
    CHECK(hangpolicy::dumpAt(uint32_t(Msg::ReleaseTexture), 0, false));
    CHECK(!hangpolicy::dumpAt(uint32_t(Msg::CreateTexture), 0, true));   // once a game
    CHECK(!hangpolicy::dumpAt(uint32_t(Msg::CreateTexture), 1, false));  // its first tell only
    CHECK(!hangpolicy::dumpAt(uint32_t(Msg::Execute), 0, false));
    CHECK(!hangpolicy::dumpAt(uint32_t(Msg::PreparePipelines), 0, false));
}
