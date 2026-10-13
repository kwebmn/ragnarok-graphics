#pragma once
#include <string>
#include <vector>

#include "core/settings/strings.hpp"

namespace uagfx::app {

std::wstring wide(const std::string& utf8);
std::string utf8(const std::wstring& w);
// The page texts (stage 2 spec §6) in the chosen language.
std::wstring wtr(settings::Lang l, settings::Text t);
std::wstring wtrf(settings::Lang l, settings::Text t, const std::vector<std::string>& args);
std::string num(long long v);  // "1920"
std::string scaleText(double s);  // "1.75", the decimal point as the language writes it is not needed: always "."

}  // namespace uagfx::app
