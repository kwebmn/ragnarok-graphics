#include "core/reference_walk.hpp"

namespace uagfx::refwalk {

std::string fileName(const Manifest& m) { return "walkable-" + m.sha256.substr(0, 12) + ".bin"; }
std::string sitePath(const Manifest& m) { return "/api/public/game/" + refwalk::fileName(m); }

}  // namespace uagfx::refwalk
