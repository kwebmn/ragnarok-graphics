#pragma once
#include <map>
#include <string>

#include "core/config.hpp"
#include "core/settings/auto_setup.hpp"

namespace uagfx::settings {

// 2.6: what the settings app recommends for every row of the schema on this computer - the auto setup's value, else
// the shipped default ("section.key" -> the word): the dots before changed rows, "Reset the page".
std::map<std::string, std::string> recommendedTokens(const Config& defaults, const AutoSetup& a);

}  // namespace uagfx::settings
