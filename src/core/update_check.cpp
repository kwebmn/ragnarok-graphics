#include "core/update_check.hpp"

#include <cstdlib>
#include <vector>

namespace uagfx::update {
namespace {

std::vector<long> numbers(const std::string& v) {
    std::vector<long> out;
    size_t at = 0;
    while (at < v.size()) {
        size_t dot = v.find('.', at);
        if (dot == std::string::npos) dot = v.size();
        out.push_back(std::strtol(v.substr(at, dot - at).c_str(), nullptr, 10));
        at = dot + 1;
    }
    return out;
}

}  // namespace

std::string versionPath(const std::string& commit) {
    return "/kwebmn/ragnarok-graphics/" + commit + "/src/core/version.cpp";
}

bool isCommit(const std::string& text) {
    if (text.size() != 40) return false;
    for (char c : text)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

std::string versionOf(const std::string& cpp) {
    const std::string mark = "kVersion = \"";
    const size_t at = cpp.find(mark);
    if (at == std::string::npos) return "";
    const size_t end = cpp.find('"', at + mark.size());
    if (end == std::string::npos) return "";
    const std::string v = cpp.substr(at + mark.size(), end - at - mark.size());
    for (char c : v)
        if (!((c >= '0' && c <= '9') || c == '.')) return "";
    return v;
}

bool newer(const std::string& a, const std::string& b) {
    if (a.empty()) return false;
    const std::vector<long> x = numbers(a), y = numbers(b);
    for (size_t i = 0; i < x.size() || i < y.size(); ++i) {
        const long p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
        if (p != q) return p > q;
    }
    return false;
}

}  // namespace uagfx::update
