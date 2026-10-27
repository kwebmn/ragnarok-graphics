#pragma once
#include <string>

#include "core/reports.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::reporttexts {

// 3.0.9-beta.4: what the player is told of an answer, in their language (UTF-8) - a status's name («Виправлено в
// 3.0.9-beta.4»), the app's strip («Є відповідь на звернення R-XXXXXX · <status> — <what to do>») and the game card's
// line when the answer came without words (a site before 3.0.9-beta.4)
std::string status(settings::Lang l, reports::Status s, const std::string& fixedIn);
std::string strip(settings::Lang l, const reports::Answer& a);
std::string hint(settings::Lang l, const reports::Answer& a);

}  // namespace uagfx::reporttexts
