#include <cmath>
#include <cstdio>
#include <string>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/social_live.hpp"

using namespace uagfx;
namespace ul = uagfx::uikit::social;

namespace {
Rml::ElementList byClass(Rml::ElementDocument* d, const char* cls) {
    Rml::ElementList l, shown;  // those shown (a data-for's template and a false data-if's hidden)
    d->GetElementsByClassName(l, cls);
    for (Rml::Element* e : l)
        if (e->IsVisible(true) && !e->HasAttribute("data-for")) shown.push_back(e);
    return shown;
}
}  // namespace

// 3.0.18-beta.141 (the owner: «все» - the friends and party card handier): its markup bound - a person's card (face
// letter, class, HP bar, where), the menu opened under its row, the friend's field on the friends' tab, the hint
TEST(uikit_social_people_cards_and_menu) {
    static ul::Model m;
    m.tab = 1;
    m.tabs = {"Друзі", "Група"};
    m.header = "CraftParty";
    m.count = "2 / 3 у мережі";
    m.exp = "Досвід: порівну";
    m.settings = "Налаштування";
    m.hint = "Права кнопка по людині - дії";
    ul::RowV a;
    a.name = "Sage", a.letter = "S", a.job = "Sage 85", a.where = "поруч · 4 кл.", a.hp = 64, a.online = true;
    a.q0 = "Шепнути";
    ul::RowV b = a;
    b.name = "Me", b.idx = 1, b.leader = true, b.hp = -1, b.q0 = "Вийти з групи";
    m.rows = {a, b};
    m.menu = {"Шепнути", "Виключити", "Копіювати ім'я"};
    m.menuRow = 0;
    std::string error;
    Rml::ElementDocument* d = rmlhost::load(ul::rml(), [&](Rml::DataModelConstructor& c) { ul::bind(c, m); }, 200,
                                            100, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    CHECK(d != nullptr);
    rmlhost::update();
    CHECK_EQ(byClass(d, "prow").size(), size_t(2));
    CHECK_EQ(byClass(d, "hpb").size(), size_t(1));  // the HP bar of the one whose HP is known
    CHECK_EQ(byClass(d, "pmenu").size(), size_t(1));
    CHECK_EQ(byClass(d, "mi").size(), size_t(3));
    const Rml::ElementList rows = byClass(d, "row"), menu = byClass(d, "pmenu");
    CHECK(menu[0]->GetAbsoluteOffset().y > rows[0]->GetAbsoluteOffset().y);  // under the first person, before the next
    CHECK(rows[1]->GetAbsoluteOffset().y > menu[0]->GetAbsoluteOffset().y);
    const Rml::ElementList fill = byClass(d, "hpf"), bar = byClass(d, "hpb");
    CHECK(std::abs(fill[0]->GetBox().GetSize().x - 0.64f * bar[0]->GetBox().GetSize().x) < 1.5f);
    CHECK(byClass(d, "addf").empty());  // the party's tab: no friend's field
    CHECK(rmlhost::log().find("rror") == std::string::npos);
    rmlhost::close(d);
}

// The friends' tab with none: what to do said, the name's field and «Додати» under the list
TEST(uikit_social_no_friends_says_how) {
    static ul::Model m;
    m.tab = 0;
    m.tabs = {"Друзі", "Група"};
    m.empty = "Друзів поки немає";
    m.nameHint = "Ім'я гравця";
    m.add = "Додати";
    std::string error;
    Rml::ElementDocument* d = rmlhost::load(ul::rml(), [&](Rml::DataModelConstructor& c) { ul::bind(c, m); }, 200,
                                            100, &error);
    CHECK_EQ(error, std::string());
    rmlhost::update();
    CHECK_EQ(byClass(d, "addf").size(), size_t(1));
    CHECK_EQ(byClass(d, "hint").size(), size_t(1));
    CHECK_EQ(byClass(d, "ph").size(), size_t(1));  // the field's cue while it is empty
    CHECK(byClass(d, "prow").empty());
    rmlhost::close(d);
}

// 3.0.24-beta.51 (the audit 28.11, C6): «Група» with no party - the name field and «Створити групу» under the hint;
// in a party (or on «Друзі») not there
TEST(uikit_social_no_party_has_a_name_field_and_a_button) {
    static ul::Model m;
    m.tab = 1;
    m.tabs = {"Друзі", "Група"};
    m.empty = "Групи немає";
    m.createOn = true;
    m.partyHint = "Назва групи";
    m.create = "Створити групу";
    std::string error;
    Rml::ElementDocument* d = rmlhost::load(ul::rml(), [&](Rml::DataModelConstructor& c) { ul::bind(c, m); }, 200,
                                            100, &error);
    CHECK_EQ(error, std::string());
    rmlhost::update();
    const Rml::ElementList fields = byClass(d, "addf");
    CHECK_EQ(fields.size(), size_t(1));
    bool button = false;
    for (Rml::Element* b : byClass(d, "btn")) button = button || b->GetInnerRML() == "Створити групу";
    CHECK(button);
    rmlhost::close(d);
}
