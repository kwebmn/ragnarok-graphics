#pragma once
#include <string>

#include "core/settings/strings.hpp"

namespace uagfx::settings {

// 2.6.22: the names the effects had before the groups by what the eye is on - the settings app's search and the
// console's find an effect by them too. nullptr: the name stayed.
const char* oldName(const std::string& sectionKey, Lang l);

}  // namespace uagfx::settings
