#pragma once
#include <string>

// 3.0.24-beta.33 (R-8BCJAH «белый интерфейс не читабелен»: the theme changed with the windows open, their words kept
// the old theme's colours): a document RmlUi made from markup in memory reloads its style sheet by reading its source
// («uaui://<model>») again - the markup kept by that source and given back by the file interface (win/uaui's, the
// host tests')
namespace uagfx::uikit::docsources {

void keep(const std::string& source, const std::string& rml);  // the markup a document was made from
bool find(const std::string& source, std::string& rml);         // false: none kept
void forget(const std::string& source);                         // its document gone

}  // namespace uagfx::uikit::docsources
