#pragma once
#include <string>

#include "core/config.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::debugviews {

// 3.0.23-beta.5 (R-FPT4TT: a tester's [debug] seams_view 2 left on for days - «артефакти»): the debug views that
// change the picture ([debug] seams_view, xray) told on the screen at each map - which and where to turn them off;
// "" when none is on
std::string note(const Config& c, settings::Lang lang);

}  // namespace uagfx::debugviews
