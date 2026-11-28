#include "core/checks_now.hpp"

#include <algorithm>
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
    CHECK_EQ(checksnow::open(done, 0).size(), n - 1);  // 3.0.24-beta.50: 0 - of every beta
    CHECK_EQ(checksnow::open("", 0).size(), n);
    CHECK_EQ(checksnow::open(",,", 0).size(), n);
    CHECK(checksnow::markDone("a", "b,c") == "a");  // no id with a comma
    CHECK(checksnow::shown("3.0.24-beta.3") && !checksnow::shown("3.0.24"));
    const std::string note = checksnow::workedNote(checksnow::all()[0], "3.0.24-beta.3");
    CHECK(note.find(first) != std::string::npos && note.find("працює") != std::string::npos);
}

// 3.0.24-beta.50 (the audit 28.11, item 11: «Перевіряємо зараз 55»): the open checks of the newest two betas alone -
// marked, the two before them come; all of them still known (the done ones by id)
TEST(checks_now_open_shows_the_newest_two_betas) {
    const auto key = [](const std::string& v) {
        std::vector<int> n;
        int x = -1;
        for (const char c : v + ".") {
            if (c >= '0' && c <= '9') x = (x < 0 ? 0 : x * 10) + (c - '0');
            else if (x >= 0) n.push_back(x), x = -1;
        }
        return n;
    };
    std::vector<std::string> betas;
    for (const checksnow::Check& c : checksnow::all())
        if (std::find(betas.begin(), betas.end(), c.since) == betas.end()) betas.push_back(c.since);
    std::sort(betas.begin(), betas.end(), [&](const std::string& a, const std::string& b) { return key(a) > key(b); });
    CHECK(betas.size() >= 4);
    if (betas.size() < 4) return;
    std::string done;
    for (int round = 0; round < 2; ++round) {
        const std::vector<const checksnow::Check*> shown = checksnow::open(done);
        size_t want = 0;
        for (const checksnow::Check& c : checksnow::all())
            if (c.since == betas[size_t(2 * round)] || c.since == betas[size_t(2 * round + 1)]) ++want;
        CHECK_EQ(shown.size(), want);
        for (const checksnow::Check* c : shown) {
            CHECK(c->since == betas[size_t(2 * round)] || c->since == betas[size_t(2 * round + 1)]);
            done = checksnow::markDone(done, c->id);
        }
    }
    CHECK(checksnow::open("", 0).size() == checksnow::all().size());  // 0: all of them
}
