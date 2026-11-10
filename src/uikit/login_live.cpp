#include "uikit/login_live.hpp"

#include <RmlUi/Core.h>

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <utility>
#include <string>

#include "uikit/hits.hpp"

namespace uagfx::uikit::login {
namespace {

namespace lc = uagfx::logincard;
namespace ly = uagfx::uikit::layered;

std::string markup() {
    std::string s = R"RML(<rml><head><link type="text/rcss" href="uaui:theme.rcss"/></head>
<body class="lw lgl" data-style-font-size="base + 'px'" data-style-background-color="ground">
<div class="back" data-if="back" data-hit="shelf" data-class-hot="shelfHot">{{shelf}}</div>
<div class="ttl">{{title}}</div><div class="sub">{{subtitle}}</div>
<div class="flbl">{{accountWord}}</div>
<div class="fld" data-hit="account" data-class-on="accountFocus" data-class-bad="accountBad"
  data-class-hot="accountHot"><span class="fv">{{account}}</span><span class="caret" data-if="accountCaret"></span><div
  class="drop" data-if="drop" data-hit="drop"><img data-if="dropSrc != ''" data-attr-src="dropSrc"
  data-style-width="dropSide + 'px'" data-style-height="dropSide + 'px'"/></div></div>
<div class="errs" data-if="errorAtAccount"><div data-attr-class="errorCls" data-hit="error">{{error}}</div><div
  class="act" data-if="errorAction != ''" data-hit="erroraction" data-class-hot="actionHot">{{errorAction}}</div></div>
<div class="flbl">{{passwordWord}}</div>
<div class="fld pw" data-hit="password" data-class-on="passwordFocus" data-class-bad="passwordBad"
  data-class-hot="passwordHot"><span class="fv">{{secret}}</span><span class="caret" data-if="passwordCaret"></span><div
  class="eye" data-hit="eye"><img data-if="eyeSrc != ''" data-attr-src="eyeSrc" data-style-width="eyeW + 'px'"
  data-style-height="eyeH + 'px'"/></div></div>
<div class="errs pwe" data-if="errorAtPassword"><div data-attr-class="errorCls" data-hit="error">{{error}}</div><div
  class="act" data-if="errorAction != ''" data-hit="erroraction" data-class-hot="actionHot">{{errorAction}}</div></div>
<div class="tk" data-hit="remember"><div class="box" data-hit="tick" data-class-on="rememberOn"><span
  data-if="rememberOn">&#x2713;</span></div><span>{{remember}}</span></div>
<div class="tk" data-hit="addshelf"><div class="box" data-hit="addtick" data-class-on="addShelfOn"><span
  data-if="addShelfOn">&#x2713;</span></div><span>{{addShelf}}</span></div>
<div class="lbtns"><div class="lb go" data-hit="enter" data-class-hot="enterHot">{{enter}}</div><div class="lb"
  data-hit="exit" data-class-hot="exitHot">{{exit}}</div></div>
<div class="links"><div class="lk" data-hit="link" data-class-hot="linkHot"
  data-class-across="!twoLinks">{{link}}</div><div
  class="lk r" data-if="twoLinks" data-hit="forgot" data-class-hot="forgotHot">{{forgot}}</div></div>
<div class="note" data-if="note != ''" data-hit="note">{{note}}</div>
<div class="site" data-if="offer" data-hit="site" data-class-hot="siteHot">{{siteOffer}}</div>
<div class="site chip" data-if="chip" data-hit="site"><span class="sc">{{siteChip}}</span><span class="so"
  data-hit="siteout" data-class-hot="siteOutHot">{{siteOut}}</span></div>
)RML";
    s += ly::rml();
    s += "\n</body></rml>";
    return s;
}

panel::Box one(Rml::ElementDocument* doc, const char* part) {
    const std::vector<Placed> p = placesOf(doc, part);
    return p.empty() ? panel::Box{} : p.front().box;
}

}  // namespace

const char* rml() {
    static const std::string s = markup();
    return s.c_str();
}

void bind(Rml::DataModelConstructor& c, Model& m) {
    for (auto [n, v] : std::initializer_list<std::pair<const char*, Rml::String*>>{
             {"ground", &m.ground},       {"title", &m.title},         {"subtitle", &m.subtitle},
             {"accountWord", &m.accountWord}, {"passwordWord", &m.passwordWord}, {"account", &m.account},
             {"secret", &m.secret},       {"remember", &m.remember},   {"addShelf", &m.addShelf},
             {"enter", &m.enter},         {"exit", &m.exit},           {"link", &m.link},
             {"forgot", &m.forgot},       {"shelf", &m.shelf},         {"error", &m.error},
             {"errorCls", &m.errorCls},   {"errorAction", &m.errorAction}, {"note", &m.note},
             {"siteOffer", &m.siteOffer}, {"siteChip", &m.siteChip},   {"siteOut", &m.siteOut},
             {"dropSrc", &m.dropSrc},     {"eyeSrc", &m.eyeSrc}})
        c.Bind(n, v);
    for (auto [n, v] : std::initializer_list<std::pair<const char*, bool*>>{
             {"accountFocus", &m.accountFocus}, {"passwordFocus", &m.passwordFocus},
             {"accountCaret", &m.accountCaret}, {"passwordCaret", &m.passwordCaret},
             {"accountBad", &m.accountBad},     {"passwordBad", &m.passwordBad},
             {"accountHot", &m.accountHot},     {"passwordHot", &m.passwordHot},
             {"rememberOn", &m.rememberOn},     {"addShelfOn", &m.addShelfOn},
             {"enterHot", &m.enterHot},         {"exitHot", &m.exitHot},
             {"linkHot", &m.linkHot},           {"forgotHot", &m.forgotHot},
             {"shelfHot", &m.shelfHot},         {"actionHot", &m.actionHot},
             {"siteHot", &m.siteHot},           {"siteOutHot", &m.siteOutHot},
             {"back", &m.back},                 {"drop", &m.drop},
             {"errorAtAccount", &m.errorAtAccount}, {"errorAtPassword", &m.errorAtPassword},
             {"twoLinks", &m.twoLinks},         {"chip", &m.chip},
             {"offer", &m.offer}})
        c.Bind(n, v);
    c.Bind("base", &m.base);
    c.Bind("eyeW", &m.eyeW);
    c.Bind("eyeH", &m.eyeH);
    c.Bind("dropSide", &m.dropSide);
    ly::bind(c, m.layer);
}

void fill(Model& m, const lc::Layout& L, const lc::Options& o, const lc::Fields& f, const lc::Show& sh,
          const lc::Texts& t) {
    const float s = L.s > 0 ? L.s : 1;
    const auto hot = [&](lc::Part p) { return sh.hover == int(p); };
    m.base = 12 * s;
    m.title = t.title;
    m.subtitle = t.subtitle;
    m.accountWord = t.account;
    m.passwordWord = t.password;
    m.account = f.account;
    std::string dots;
    for (int i = 0; i < std::clamp(f.passwordLength, 0, 24); ++i) dots += "\xE2\x80\xA2";
    m.secret = f.passwordShown.empty() ? dots : f.passwordShown;  // the eye held: its letters
    m.accountFocus = f.focus == lc::Focus::Account;
    m.passwordFocus = f.focus == lc::Focus::Password;
    m.accountCaret = m.accountFocus && f.caret;
    m.passwordCaret = m.passwordFocus && f.caret;
    const bool bad = !f.error.empty();
    m.accountBad = bad && f.errorField == loginerr::Field::Account;
    m.passwordBad = bad && f.errorField == loginerr::Field::Password;
    m.accountHot = hot(lc::Part::Account);
    m.passwordHot = hot(lc::Part::Password);
    m.remember = t.remember;
    m.rememberOn = f.remember;
    m.addShelf = t.addShelf;
    m.addShelfOn = f.addShelf;
    m.enter = t.enter;
    m.exit = t.exit;
    m.enterHot = hot(lc::Part::Enter);
    m.exitHot = hot(lc::Part::Exit);
    m.twoLinks = !t.forgot.empty();
    m.link = t.register_;
    m.forgot = t.forgot;
    m.linkHot = hot(lc::Part::Register);
    m.forgotHot = hot(lc::Part::Forgot);
    m.back = o.back;
    m.shelf = t.shelf;
    m.shelfHot = hot(lc::Part::Shelf);
    m.drop = o.drop;
    // the refusal (red, with what to do) or the typing's hint (amber) under the field it is about - its place as core
    // logincard's layout has it (the error's line only when it has one)
    const bool line = o.errorAt != loginerr::Field::None && (bad || !f.hint.empty());
    const loginerr::Field at = o.errorAt;
    m.error = bad ? f.error : f.hint;
    m.errorCls = bad ? "err" : "err hint";
    m.errorAction = !bad                                                         ? ""
                    : f.errorAction == loginerr::Action::CreateAccount   ? t.createAccount
                    : f.errorAction == loginerr::Action::ResetPassword ? t.resetPassword
                                                                         : "";
    m.actionHot = hot(lc::Part::ErrorAction);
    m.errorAtAccount = line && at == loginerr::Field::Account;
    m.errorAtPassword = line && at == loginerr::Field::Password;
    m.note = o.note ? f.note : "";
    m.offer = o.site && f.site == lc::SiteLine::Offer;
    m.chip = o.site && f.site == lc::SiteLine::Chip;
    m.siteOffer = t.siteOffer;
    m.siteChip = f.siteChip;
    m.siteOut = t.siteOut;
    m.siteHot = hot(lc::Part::Site);
    m.siteOutHot = hot(lc::Part::SiteOut);
}

bool refit(Rml::ElementDocument* doc, lc::Layout& L) {
    if (!doc || !doc->IsVisible()) return false;
    const Rml::Vector2f size = doc->GetBox().GetSize(Rml::BoxArea::Border);
    const panel::Box account = one(doc, "account"), password = one(doc, "password");
    if (size.x <= 0 || size.y <= 0 || account.w <= 0 || password.w <= 0) return false;
    L.account = account;  // by name - never by order
    L.password = password;
    L.remember = one(doc, "remember");
    L.tick = one(doc, "tick");
    L.addShelf = one(doc, "addshelf");
    L.addTick = one(doc, "addtick");
    L.enter = one(doc, "enter");
    L.exit = one(doc, "exit");
    L.link = one(doc, "link");
    L.forgot = one(doc, "forgot");
    L.back = one(doc, "shelf");
    L.drop = one(doc, "drop");
    L.eye = one(doc, "eye");
    L.error = one(doc, "error");
    L.errorAction = one(doc, "erroraction");
    if (L.errorAction.w > 0 && L.errorAction.h > 0 && L.error.h <= 0) L.errorAction = panel::Box{};
    L.note = one(doc, "note");
    L.site = one(doc, "site");
    L.siteOut = one(doc, "siteout");
    L.glass = panel::Box{0, 0, size.x, size.y};
    L.w = int(std::ceil(size.x));
    L.h = int(std::ceil(size.y));
    return true;
}

}  // namespace uagfx::uikit::login
