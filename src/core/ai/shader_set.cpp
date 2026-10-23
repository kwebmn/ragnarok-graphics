#include "core/ai/shader_set.hpp"

namespace uagfx::ai {

std::vector<std::string> shadersFor(bool network) {
    if (!network) return {"ai_mip"};
    return {"ai_conv", "ai_shuffle", "ai_mip", "ai_alpha", "ai_place"};
}

}  // namespace uagfx::ai
