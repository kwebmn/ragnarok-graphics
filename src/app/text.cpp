#include "app/text.hpp"

#include <cstdio>

namespace uagfx::app {

std::wstring wtr(settings::Lang l, settings::Text t) { return wide(settings::tr(l, t)); }

std::wstring wtrf(settings::Lang l, settings::Text t, const std::vector<std::string>& args) {
    return wide(settings::fmt(settings::tr(l, t), args));
}

std::string num(long long v) { return std::to_string(v); }

std::string scaleText(double s, settings::Lang l) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.2f", s);
    std::string out = buf;
    while (out.size() > 1 && out.back() == '0') out.pop_back();
    if (!out.empty() && out.back() == '.') out.pop_back();
    if (l != settings::Lang::En)  // as the spec writes it: "×1,75" (0.6.0 review M15)
        for (char& ch : out)
            if (ch == '.') ch = ',';
    return out;
}

}  // namespace uagfx::app
