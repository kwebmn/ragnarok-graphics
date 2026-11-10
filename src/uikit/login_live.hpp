#pragma once
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/login_card.hpp"
#include "uikit/layered.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::login {

// 3.0.18-beta.107 (the owner: «делай 1-3», the login screens): the login card laid out by RmlUi - «‹ Полиця», the
// title and its line, «Логін» (▾ of the saved logins in its field), the refusal under the field it is about, «Пароль»
// (the eye in its field), the two ticks, «Увійти» | «Вийти», «Створити акаунт» | «Забули пароль?», the note, the site
// account's line. refit() puts RmlUi's boxes into core logincard's layout BY NAME (account into account, password into
// password): the card's clicks stay core logincard's own (feedback: a field's role never from an order).
struct Model {
    float base = 12;
    Rml::String ground = "#00000000", title, subtitle, accountWord, passwordWord, account, secret, remember,
                addShelf, enter, exit, link, forgot, shelf, error, errorCls = "err", errorAction, note, siteOffer,
                siteChip, siteOut, dropSrc, eyeSrc;
    bool accountFocus = false, passwordFocus = false, accountCaret = false, passwordCaret = false;
    bool accountBad = false, passwordBad = false, accountHot = false, passwordHot = false;
    bool rememberOn = false, addShelfOn = false, enterHot = false, exitHot = false, linkHot = false,
         forgotHot = false, shelfHot = false, actionHot = false, siteHot = false, siteOutHot = false;
    bool back = false, drop = false, errorAtAccount = false, errorAtPassword = false, twoLinks = true,
         chip = false, offer = false;
    float eyeW = 0, eyeH = 0, dropSide = 0;
    layered::Layer layer;  // ▾'s list over the card (empty when closed)
};

const char* rml();
void bind(Rml::DataModelConstructor& c, Model& m);
// `o` what core logincard's layout was asked for (the refusal's field, the note, «‹ Полиця», ▾, the site's line) -
// never read back from the laid-out card (a part hidden once would stay hidden)
void fill(Model& m, const logincard::Layout& L, const logincard::Options& o, const logincard::Fields& f,
          const logincard::Show& sh, const logincard::Texts& t);
// RmlUi's boxes into the card's core layout by their names; x, y kept (core logincard places the card)
bool refit(Rml::ElementDocument* doc, logincard::Layout& L);

}  // namespace uagfx::uikit::login
