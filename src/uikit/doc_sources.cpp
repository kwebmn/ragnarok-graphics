#include "uikit/doc_sources.hpp"

#include <map>

namespace uagfx::uikit::docsources {
namespace {

std::map<std::string, std::string>& kept() {  // the documents' thread's (the game's, the tests')
    static std::map<std::string, std::string> m;
    return m;
}

}  // namespace

void keep(const std::string& source, const std::string& rml) { kept()[source] = rml; }

bool find(const std::string& source, std::string& rml) {
    const auto it = kept().find(source);
    if (it == kept().end()) return false;
    rml = it->second;
    return true;
}

void forget(const std::string& source) { kept().erase(source); }

}  // namespace uagfx::uikit::docsources
