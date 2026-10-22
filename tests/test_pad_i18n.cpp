#include "pad/core/config/bindings.hpp"
#include "pad/core/i18n.hpp"
#include "test.hpp"

using namespace uapad;

TEST(i18n_every_text_in_every_language) {
    for (int t = 0; t < int(Text::Count); ++t)
        for (Lang l : {Lang::En, Lang::Uk, Lang::Ru}) {
            const char* s = tr(l, Text(t));
            CHECK(s != nullptr && s[0] != '\0');
        }
    CHECK(std::string(tr(Lang::En, Text::Save)) == "Save");
    CHECK(std::string(tr(Lang::Uk, Text::Save)) == "Зберегти");
    CHECK(std::string(tr(Lang::Ru, Text::Save)) == "Сохранить");
}

TEST(i18n_language_tokens) {
    CHECK(parseLang("en") && *parseLang("en") == Lang::En);
    CHECK(parseLang(" UK ") && *parseLang(" UK ") == Lang::Uk);
    CHECK(parseLang("ru") && *parseLang("ru") == Lang::Ru);
    CHECK(!parseLang("de"));
    CHECK(!parseLang(""));
    CHECK(std::string(langToken(Lang::Uk)) == "uk");
    CHECK(std::string(langName(Lang::En)) == "English");
    CHECK(std::string(langName(Lang::Uk)) == "Українська");
    CHECK(std::string(langName(Lang::Ru)) == "Русский");
}

TEST(i18n_action_labels) {
    const Action slot12{Action::Kind::Slot, 11};
    CHECK(actionLabel(slot12, Lang::En) == "Slot F3, row 2");
    CHECK(actionLabel(slot12, Lang::Uk) == "Слот F3, ряд 2");
    CHECK(actionLabel(slot12, Lang::Ru) == "Слот F3, ряд 2");
    CHECK(actionLabel(Action{Action::Kind::Slot, 0}, Lang::En) == "Slot F1");
    CHECK(actionLabel(Action{Action::Kind::Attack, 0}, Lang::En) == "Attack");
    CHECK(actionLabel(Action{Action::Kind::Pickup, 0}, Lang::Uk) == "Підібрати");
    CHECK(actionLabel(Action{Action::Kind::SitStand, 0}, Lang::Ru) == "Сесть / встать");
    CHECK(actionLabel(Action{Action::Kind::ClearTarget, 0}, Lang::En) == "Clear target");
    CHECK(actionLabel(Action{}, Lang::Uk) == "—");
}
