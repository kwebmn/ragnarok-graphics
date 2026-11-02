#include "core/register_form.hpp"

namespace uagfx::regform {
namespace {

bool loginOk(const std::string& s) {  // ^[A-Za-z0-9_-]{4,23}$
    if (s.size() < 4 || s.size() > 23) return false;
    for (const char c : s)
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
            return false;
    return true;
}

bool emailOk(const std::string& s) {  // x@y.z: one @, a dot after it, none at the ends, no spaces
    const size_t at = s.find('@');
    if (at == 0 || at == std::string::npos || s.find('@', at + 1) != std::string::npos) return false;
    const size_t dot = s.find('.', at + 2);
    return dot != std::string::npos && dot + 1 < s.size() && s.find(' ') == std::string::npos && s.size() <= 191;
}

}  // namespace

std::vector<std::pair<Field, settings::Text>> check(const Form& f) {
    std::vector<std::pair<Field, settings::Text>> out;
    if (!loginOk(f.login)) out.emplace_back(Field::Login, settings::Text::RegErrLogin);
    if (f.password.size() < 8 || f.password.size() > 32)
        out.emplace_back(Field::Password, settings::Text::RegErrPassword);
    else if (f.password2 != f.password)
        out.emplace_back(Field::Password2, settings::Text::RegErrPassword2);
    if (!emailOk(f.email)) out.emplace_back(Field::Email, settings::Text::RegErrEmail);
    if (f.sex != 0 && f.sex != 1) out.emplace_back(Field::Sex, settings::Text::RegErrSex);
    if (!f.agree) out.emplace_back(Field::Agree, settings::Text::RegErrAgree);
    return out;
}

}  // namespace uagfx::regform
