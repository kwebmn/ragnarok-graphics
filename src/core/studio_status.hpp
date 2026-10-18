#pragma once
#include "core/json.hpp"
#include "core/pack_build.hpp"

namespace uagfx::studio {

// 2.3: a texture pack build's status (pack::Status) in uaRO Studio's engine's progress replies - apart from
// core/studio_link.hpp, which the driver's code takes (pack_build.hpp's members meet Windows' COM macros there).
json::Value statusJson(const pack::Status& s);
bool statusOfJson(const json::Value& v, pack::Status& out);  // false: no state in it

}  // namespace uagfx::studio
