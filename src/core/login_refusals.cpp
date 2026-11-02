#include "core/login_refusals.hpp"

namespace uagfx::loginerr {

Refusal refusal(int code) {
    Refusal r;
    switch (code) {
        case 0: return Refusal{Field::Account, settings::Text::LoginErrUnknownId, Action::CreateAccount, true};
        case 1: return Refusal{Field::Password, settings::Text::LoginErrPassword, Action::ResetPassword, true};
        case 2: r.text = settings::Text::LoginErrExpired; break;
        case 4: r.text = settings::Text::LoginErrMaintenance; break;
        case 5: r.text = settings::Text::LoginErrOldExe; break;
        case 6: r.text = settings::Text::LoginErrBannedUntil; break;
        case 7: r.text = settings::Text::LoginErrJammed; break;
        case 99: r.text = settings::Text::LoginErrRemoved; break;
        default: break;
    }
    return r;
}

std::string words(const Refusal& r, settings::Lang lang, const std::string& login, const std::string& until) {
    const bool dated = r.text == settings::Text::LoginErrBannedUntil;
    return settings::fmt(settings::tr(lang, r.text), {dated ? until : login});
}

}  // namespace uagfx::loginerr
