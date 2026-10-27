// 3.0.9-beta.4: uaro-graphics\reports\answers.json - the watcher writes the answers not seen yet, the driver shows them
// (core/report_answers)
#include <string>
#include <vector>

#include "core/report_answers.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(report_answers_round_trip) {
    std::vector<reports::Answer> list(2);
    list[0].id = 218;
    list[0].code = "R-7BDUE3";
    list[0].text = "Надішліть \"з Pause\"\nдякую \\ ok";
    list[0].at = "2026-10-27T00:31:00+00:00";
    list[0].status = reports::Status::NeedInfo;
    list[1].code = "R-6JBVTG";
    list[1].status = reports::Status::Fixed;
    list[1].fixedIn = "3.0.9-beta.3";
    list[1].at = "2026-10-27T08:19:00+00:00";
    std::vector<reports::Answer> back;
    CHECK(answers::read(answers::write(list), back));
    CHECK_EQ(back.size(), size_t(2));
    if (back.size() != 2) return;
    CHECK_EQ(back[0].id, 218LL);
    CHECK_EQ(back[0].code, std::string("R-7BDUE3"));
    CHECK_EQ(back[0].text, list[0].text);
    CHECK_EQ(back[0].at, list[0].at);
    CHECK(back[0].status == reports::Status::NeedInfo);
    CHECK_EQ(back[1].id, 0LL);
    CHECK(back[1].status == reports::Status::Fixed);
    CHECK_EQ(back[1].fixedIn, std::string("3.0.9-beta.3"));
    // what tells one answer from another: its id, else (an older site) its time
    CHECK(answers::key(back[0]) != answers::key(back[1]));
    CHECK_EQ(answers::key(back[0]), std::string("R-7BDUE3#218"));
    CHECK_EQ(answers::key(back[1]), std::string("R-6JBVTG@2026-10-27T08:19:00+00:00"));
    std::vector<reports::Answer> none;
    CHECK(answers::read(answers::write({}), none));
    CHECK(none.empty());
}

// The file is read while the watcher may be writing it, and its code goes to a command line: only the site's codes
TEST(report_answers_refuse_what_is_not_theirs) {
    std::vector<reports::Answer> out;
    CHECK(!answers::read("", out));
    CHECK(!answers::read("{}", out));
    CHECK(!answers::read("{\"answers\":5}", out));
    CHECK(!answers::read("[1,2]", out));
    CHECK(!answers::read("{\"answers\":[{\"code\":\"R-AB", out));
    CHECK(answers::read("{\"answers\":[{\"code\":\"R-7BDUE3\"},{\"code\":\"R-7BDUE3 --x\"},{\"code\":\"r-7bdue3\"},"
                        "{\"code\":\"R-7BDU\"},{\"text\":\"no code\"}]}",
                        out));
    CHECK_EQ(out.size(), size_t(1));
    CHECK(answers::plainCode("R-ZZT0K1"));
    CHECK(!answers::plainCode("R-ZZT0K1\""));
    CHECK(!answers::plainCode("R-zzt0k1"));
    CHECK(!answers::plainCode(""));
}
