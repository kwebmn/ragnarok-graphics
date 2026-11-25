#include "core/checks_now.hpp"

#include <set>

#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.24-beta.3 (the owner: «пункт, в котором если мы чтото проверяем он плявлялся там ... после проверок уберать
// оттуда», «3 да», «4 отправляет»): the checks - each its own id, words in each language, a real switch when it names
// one; marked done, a check leaves the list; only a beta build shows them
TEST(checks_now_list_marks_and_the_beta_alone) {
    std::set<std::string> ids;
    for (const checksnow::Check& c : checksnow::all()) {
        CHECK(ids.insert(c.id).second);
        CHECK(std::string(c.since).rfind("3.0.", 0) == 0);
        for (int l = 0; l < 3; ++l) CHECK(std::string(c.title[l]).size() > 3 && std::string(c.steps[l]).size() > 10);
        if (*c.key) CHECK(settings::findSetting(c.key) != nullptr);
    }
    const size_t n = checksnow::all().size();
    CHECK(n >= 1);
    const std::string first = checksnow::all()[0].id;
    std::string done = checksnow::markDone("", first);
    CHECK(done == first);
    CHECK(checksnow::markDone(done, first) == first);  // once
    CHECK_EQ(checksnow::open(done).size(), n - 1);
    CHECK_EQ(checksnow::open("").size(), n);
    CHECK_EQ(checksnow::open(",,").size(), n);
    CHECK(checksnow::markDone("a", "b,c") == "a");  // no id with a comma
    CHECK(checksnow::shown("3.0.24-beta.3") && !checksnow::shown("3.0.24"));
    const std::string note = checksnow::workedNote(checksnow::all()[0], "3.0.24-beta.3");
    CHECK(note.find(first) != std::string::npos && note.find("працює") != std::string::npos);
}
