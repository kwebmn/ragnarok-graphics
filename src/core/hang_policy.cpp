#include "core/hang_policy.hpp"

#include "core/bridge_protocol.hpp"

namespace uagfx::hangpolicy {
namespace {

bool textureMessage(uint32_t message) {
    return message == uint32_t(bridge::Msg::CreateTexture) || message == uint32_t(bridge::Msg::ReleaseTexture);
}

}  // namespace

const std::vector<uint64_t>& tellsFor(uint32_t message) {
    static const std::vector<uint64_t> texture = {1000, 2000, 6000, 20000}, other = {2000, 6000, 20000};
    return textureMessage(message) ? texture : other;
}

bool dumpAt(uint32_t message, size_t tell, bool dumped) { return textureMessage(message) && tell == 0 && !dumped; }

}  // namespace uagfx::hangpolicy
