// 3.0.18-beta.244 («Довідник» › «Навички»): the site's skills' words - its answer read, our file written and read
// back, a skill's lines in a language (the English when that one has none)
#include <map>
#include <string>
#include <vector>

#include "core/reference_skills.hpp"
#include "test.hpp"

using namespace uagfx;
using settings::Lang;

TEST(reference_skills_words_from_the_site_and_back) {
    std::map<int, std::string> uk, en;
    CHECK(refskills::fromSite(R"({"descs":{"5":"Ближня атака.\nДругий рядок.","7":"Вогняний удар."},"mtime":7})", uk));
    CHECK(uk.size() == 2 && uk[5] == "Ближня атака.\nДругий рядок.");
    CHECK(!refskills::fromSite(R"({"skills":[]})", en));  // not the site's words
    CHECK(refskills::fromSite(R"({"descs":{"5":"A melee attack.","19":"Fire bolts."},"mtime":7})", en));
    refskills::File f;
    f.at = 1800000000;
    f.words[int(Lang::Uk)] = uk;
    f.words[int(Lang::En)] = en;
    refskills::File back;
    CHECK(refskills::read(refskills::write(f), back));
    CHECK(back.at == f.at && back.words[int(Lang::Uk)] == uk && back.words[int(Lang::En)] == en);
    CHECK(back.words[int(Lang::Ru)].empty());
    CHECK((refskills::lines(back, Lang::Uk, 5) == std::vector<std::string>{"Ближня атака.", "Другий рядок."}));
    CHECK((refskills::lines(back, Lang::Uk, 19) == std::vector<std::string>{"Fire bolts."}));  // the English one
    CHECK((refskills::lines(back, Lang::Ru, 5) == std::vector<std::string>{"A melee attack."}));
    CHECK(refskills::lines(back, Lang::Uk, 1).empty());
    CHECK(refskills::fresh(back, f.at + 3600) && !refskills::fresh(back, f.at + 2 * 86400));
    CHECK(!refskills::read("not json", back));
}
