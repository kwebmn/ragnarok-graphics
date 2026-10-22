// 2.13: «Звернення» - the app's side over the fake site (tests/fake_windows: web for the GETs, postAnswers for the
// POSTs, posts records what went).
#include <string>
#include <vector>

#include "app/files.hpp"
#include "app/reports.hpp"
#include "fake_windows.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

const std::wstring kKeyFile = L"uaro-graphics\\reports\\computer.txt";

bool hexKey(const std::string& key) {
    return key.size() == 32 && key.find_first_not_of("0123456789abcdef") == std::string::npos;
}

size_t queued() {
    size_t n = 0;
    for (const auto& f : fake::windows().files)
        if (f.first.rfind(fake::path(L"uaro-graphics\\reports\\queued-"), 0) == 0) ++n;
    return n;
}

}  // namespace

TEST(the_computer_key_is_made_once) {
    fake::Windows& w = fake::reset();
    const std::wstring dir = app::exeFolder();
    const std::string key = app::computerKey(dir);  // a 2.x install: no key yet
    CHECK(hexKey(key));
    CHECK_EQ(w.files[fake::path(kKeyFile)], key);
    CHECK_EQ(app::computerKey(dir), key);  // kept
    w.files[fake::path(kKeyFile)] = "not a key\r\n";
    const std::string other = app::computerKey(dir);  // a broken one is made anew
    CHECK(hexKey(other) && other != key);
    w.files[fake::path(kKeyFile)] = key + "\r\n";  // a key written by hand, with its line's end
    CHECK_EQ(app::computerKey(dir), key);
}

TEST(a_report_without_the_site_waits_and_goes_later) {
    fake::Windows& w = fake::reset();
    const std::wstring dir = app::exeFolder();
    report::Upload u;
    u.comment = "Вода мерехтить";
    u.kind = "graphics";
    u.computer = app::computerKey(dir);
    app::NewReport r = app::sendNew(dir, u);  // no answer from the site
    CHECK(r.code.empty() && r.queued && !r.error.empty());
    CHECK_EQ(queued(), size_t(1));
    CHECK_EQ(app::queuedReports(dir), 1);
    CHECK_EQ(app::sendQueued(dir), 0);  // still no answer: it stays
    CHECK_EQ(queued(), size_t(1));

    w.postAnswers["/api/graphics/reports"] = {201, "{\"code\":\"R-ABCDEF\"}"};
    CHECK_EQ(app::sendQueued(dir), 1);
    CHECK_EQ(queued(), size_t(0));
    CHECK_EQ(w.posts.size(), size_t(3));  // the first try, the one without an answer, the one that went
    CHECK(w.posts.back().body.find("\"comment\":\"Вода мерехтить\"") != std::string::npos);
    CHECK(w.posts.back().body.find("\"kind\":\"graphics\"") != std::string::npos);
    const std::vector<std::string> sent = app::sentCodesIn(dir, 5);
    CHECK(!sent.empty() && sent[0] == "R-ABCDEF");

    // the site refuses one (too big, a bad field): it never goes, so it is not kept
    w.postAnswers["/api/graphics/reports"] = {422, "{\"message\":\"bad\"}"};
    r = app::sendNew(dir, u);
    CHECK(r.code.empty() && !r.queued && !r.error.empty());
    CHECK_EQ(queued(), size_t(0));
    // a full hour (the limit): kept for later
    w.postAnswers["/api/graphics/reports"] = {429, "{\"message\":\"Too Many Attempts.\"}"};
    r = app::sendNew(dir, u);
    CHECK(r.code.empty() && r.queued);
    CHECK_EQ(queued(), size_t(1));
    w.postAnswers["/api/graphics/reports"] = {201, "{\"code\":\"R-GHJKMN\"}"};
    r = app::sendNew(dir, u);  // sent at once, and the waiting one goes with it
    CHECK_EQ(r.code, std::string("R-GHJKMN"));
    CHECK(!r.queued);
    CHECK_EQ(queued(), size_t(0));
}

TEST(the_list_and_the_conversation_come_from_the_site) {
    fake::Windows& w = fake::reset();
    const std::wstring dir = app::exeFolder();
    const std::string key = app::computerKey(dir);
    w.web["/api/graphics/reports/mine?computer=" + key] =
        "{\"reports\":[{\"code\":\"R-ABCDEF\",\"kind\":\"slow\",\"reason\":\"manual\",\"status\":\"fixing\","
        "\"title\":\"x\",\"auto\":false,\"at\":\"2026-10-22T10:00:00+00:00\",\"unread\":1}]}";
    w.web["/api/graphics/reports/R-ABCDEF?computer=" + key] =
        "{\"code\":\"R-ABCDEF\",\"status\":\"fixed\",\"fixed_in\":\"2.13.1\",\"messages\":[]}";
    std::vector<reports::Summary> list;
    std::string error;
    CHECK(app::listReports(key, list, error));
    CHECK(list.size() == 1 && list[0].unread == 1);
    reports::Conversation c;
    CHECK(app::openReport(key, "R-ABCDEF", c, error));
    CHECK(c.status == reports::Status::Fixed);
    CHECK(!app::openReport(key, "R-ZZZZZZ", c, error));  // not there: false and why
    CHECK(!error.empty());

    w.postAnswers["/api/graphics/reports/R-ABCDEF/messages"] = {201, "{\"ok\":true}"};
    CHECK(app::answerReport(key, "R-ABCDEF", "Ще \"раз\"\n", {{"uaro-1.jpg", "\xFF\xD8\xFFjpeg"}}, error));
    const std::string& body = w.posts.back().body;
    CHECK(body.find("\"computer\":\"" + key + "\"") != std::string::npos);
    CHECK(body.find("\"text\":\"Ще \\\"раз\\\"\\n\"") != std::string::npos);
    CHECK(body.find("{\"name\":\"uaro-1.jpg\",\"data\":\"/9j/anBlZw==\"}") != std::string::npos);

    w.postAnswers["/api/graphics/reports/R-ABCDEF/verdict"] = {200, "{\"ok\":true}"};
    CHECK(app::sendVerdict(key, "R-ABCDEF", true, "", error));
    CHECK(w.posts.back().body.find("\"fixed\":true") != std::string::npos);
    w.postAnswers["/api/graphics/reports/R-ABCDEF/verdict"] = {404, "{}"};
    CHECK(!app::sendVerdict(key, "R-ABCDEF", false, "", error));

    w.web["/api/graphics/reports/R-ABCDEF/shots/m4-a.png?computer=" + key] = "\x89PNG";
    std::string bytes;
    CHECK(app::fetchShot(key, "R-ABCDEF", "m4-a.png", bytes, error));
    CHECK_EQ(bytes, std::string("\x89PNG"));
}
