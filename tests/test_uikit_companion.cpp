#include <cmath>
#include <cstdio>

#include "rml_host.hpp"
#include "test.hpp"
#include "uikit/companion_live.hpp"

using namespace uagfx;
namespace hc = uagfx::homuncard;
namespace mc = uagfx::merccard;
namespace cl = uagfx::uikit::companion;

namespace {
panel::Box mid(const panel::Box& b) { return panel::Box{b.x + b.w / 2, b.y + b.h / 2, 0, 0}; }
}  // namespace

// 3.0.18-beta.98: the homunculus' card laid out by RmlUi - the core hit test on its boxes finds ×, «Нагодувати»,
// a skill's «+», ‹ ›, its row and «Видалити»
TEST(uikit_homun_card_refit_finds_its_buttons_and_skills) {
    hc::Info i;
    i.name = "Lif";
    i.level = 12;
    i.maxHp = 100, i.hp = 50;
    i.points = 2;
    i.skills = {hc::SkillRow{"Healing Hands", 3, true, 2}, hc::SkillRow{"Urgent Escape", 1, false, 1}};
    hc::Texts t;
    t.title = "Гомункул";
    t.level = "Рівень {1}";
    t.feed = "Нагодувати";
    t.remove = "Видалити";
    t.skillsHead = "Навички · вільних очок: {1}";
    t.skillLevel = "Рів. {1}";
    hc::Show sh;
    static cl::Model m;
    cl::fillHomun(m, i, sh, t, 1);
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(cl::rml(), [&](Rml::DataModelConstructor& c) { cl::bind(c, m); }, 600, 100, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    hc::Layout L = hc::layout(1920, 1080, 1, int(i.skills.size()));
    CHECK(cl::refitHomun(d, L));
    CHECK(std::abs(float(L.w) - 320) < 2.5f);
    hc::Hit h = hc::hitTest(L, mid(L.close).x, mid(L.close).y);
    CHECK(h.part == hc::Part::Button && h.button == hc::Button::Close);
    h = hc::hitTest(L, mid(L.buttons[0]).x, mid(L.buttons[0]).y);
    CHECK(h.part == hc::Part::Button && h.button == hc::Button::Feed);
    h = hc::hitTest(L, mid(L.skillPlus[0]).x, mid(L.skillPlus[0]).y);
    CHECK(h.part == hc::Part::Plus);
    CHECK_EQ(h.index, 0);
    CHECK(L.skillPlus[1].w <= 0);  // no point's leave: no «+» to hit
    h = hc::hitTest(L, mid(L.skillLess[0]).x, mid(L.skillLess[0]).y);
    CHECK(h.part == hc::Part::Less);
    h = hc::hitTest(L, mid(L.skillMore[0]).x, mid(L.skillMore[0]).y);
    CHECK(h.part == hc::Part::More);
    CHECK(L.skillLess[1].w <= 0);  // level 1: no arrows
    h = hc::hitTest(L, L.skillRows[1].x + 20, mid(L.skillRows[1]).y);
    CHECK(h.part == hc::Part::Skill);
    CHECK_EQ(h.index, 1);
    h = hc::hitTest(L, mid(L.remove).x, mid(L.remove).y);
    CHECK(h.part == hc::Part::Button && h.button == hc::Button::Delete);
    CHECK(L.skillRows[1].y > L.skillRows[0].y);
    CHECK(L.remove.y > L.skillRows[1].y);
    rmlhost::close(d);
}

// The mercenary's: its four facts, its skills without «+», «Звільнити»
TEST(uikit_merc_card_refit_finds_dismiss_and_skills) {
    mc::Info i;
    i.name = "Archer";
    i.level = 5;
    i.ends = "12:30";
    i.skills = {mc::SkillRow{"Double Strafe", 2, 2, true}};
    mc::Texts t;
    t.kinds[0] = t.kinds[1] = t.kinds[2] = "Найманець";
    t.level = "Рівень {1}";
    t.facts[0] = "Контракт до";
    t.facts[1] = "Вірність";
    t.facts[2] = "Викликів";
    t.facts[3] = "Убито";
    t.skills = "Навички";
    t.dismiss = "Звільнити";
    t.skillLevel = "Рів. {1}";
    mc::Show sh;
    static cl::Model m;
    cl::fillMerc(m, i, sh, t, 1);
    CHECK_EQ(m.facts.size(), size_t(4));
    std::string error;
    Rml::ElementDocument* d =
        rmlhost::load(cl::rml(), [&](Rml::DataModelConstructor& c) { cl::bind(c, m); }, 600, 100, &error);
    if (!error.empty()) std::printf("RmlUi: %s", error.c_str());
    CHECK_EQ(error, std::string());
    mc::Layout L = mc::layout(1920, 1080, 1, int(i.skills.size()));
    CHECK(cl::refitMerc(d, L));
    mc::Hit h = mc::hitTest(L, mid(L.dismiss).x, mid(L.dismiss).y);
    CHECK(h.part == mc::Part::Button && h.button == mc::Button::Dismiss);
    h = mc::hitTest(L, mid(L.skillLess[0]).x, mid(L.skillLess[0]).y);
    CHECK(h.part == mc::Part::Less);
    h = mc::hitTest(L, L.skillRows[0].x + 20, mid(L.skillRows[0]).y);
    CHECK(h.part == mc::Part::Skill);
    rmlhost::close(d);
}
