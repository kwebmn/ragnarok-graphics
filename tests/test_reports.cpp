// 2.13: «Звернення» - the site's answers (GET /api/graphics/reports/mine and /{code}) as core/reports reads them.
#include <string>
#include <vector>

#include "core/reports.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// As Laravel writes it: non-ASCII as \u escapes, slashes as "\/", null for what is missing.
const char* const kList =
    "{\"reports\":["
    "{\"code\":\"R-7K3F2A\",\"kind\":\"graphics\",\"reason\":\"manual\",\"status\":\"fixed\",\"fixed_in\":\"2.13.1\","
    "\"title\":\"\\u0412\\u043e\\u0434\\u0430\",\"auto\":false,\"at\":\"2026-10-22T18:57:32+00:00\","
    "\"updated\":\"2026-10-23T09:05:00+00:00\",\"unread\":2},"
    "{\"code\":\"R-AB2CD3\",\"kind\":null,\"reason\":\"crash\",\"status\":\"new\",\"fixed_in\":null,\"title\":\"\","
    "\"auto\":true,\"at\":\"2026-10-21T10:00:00+00:00\",\"updated\":\"2026-10-21T10:00:00+00:00\",\"unread\":0},"
    "{\"code\":\"\",\"status\":\"new\"},"
    "{\"code\":\"R-ZZZZZZ\",\"kind\":\"other\",\"reason\":\"manual\",\"status\":\"need_info\",\"unread\":1}]}";

const char* const kConversation =
    "{\"code\":\"R-7K3F2A\",\"kind\":\"slow\",\"reason\":\"manual\",\"status\":\"fixed\",\"fixed_in\":\"2.13.1\","
    "\"version\":\"2.13.0\",\"auto\":false,\"at\":\"2026-10-22T18:57:32+00:00\",\"messages\":["
    "{\"author\":\"player\",\"text\":\"\\u0413\\u0430\\u043b\\u044c\\u043c\\u0443\\u0454\\n\\u0432 \\\"\\u041f\\u0440"
    "\\u043e\\u043d\\u0442\\u0435\\u0440\\u0456\\\"\",\"status\":null,\"shots\":[\"uaro-1.jpg\",\"m4-uaro-2.jpg\"],"
    "\"at\":\"2026-10-22T18:57:32+00:00\"},"
    "{\"author\":\"dev\",\"text\":\"\\u0414\\u044f\\u043a\\u0443\\u0454\\u043c\\u043e \\ud83d\\ude42\",\"status\":"
    "\"fixed\",\"shots\":[],\"at\":\"2026-10-23T09:05:00+00:00\"},"
    "{\"author\":\"dev\",\"text\":\"\",\"status\":\"closed\",\"shots\":[\"..\\/..\\/secret.jpg\"],\"at\":\"\"}]}";

}  // namespace

TEST(reports_read_the_list) {
    std::vector<reports::Summary> list;
    std::string error;
    CHECK(reports::parseList(kList, list, error));
    CHECK_EQ(list.size(), size_t(3));  // the one without a code is dropped
    CHECK_EQ(list[0].code, std::string("R-7K3F2A"));
    CHECK(list[0].kind == reports::Kind::Graphics);
    CHECK(list[0].status == reports::Status::Fixed);
    CHECK_EQ(list[0].fixedIn, std::string("2.13.1"));
    CHECK_EQ(list[0].title, std::string("Вода"));
    CHECK_EQ(list[0].reason, std::string("manual"));
    CHECK(!list[0].automatic);
    CHECK_EQ(list[0].at, std::string("2026-10-22T18:57:32+00:00"));
    CHECK_EQ(list[0].updated, std::string("2026-10-23T09:05:00+00:00"));
    CHECK_EQ(list[0].unread, 2);
    CHECK(list[1].kind == reports::Kind::None);
    CHECK(list[1].automatic);
    CHECK(list[1].status == reports::Status::New);
    CHECK(list[2].status == reports::Status::NeedInfo);
    CHECK_EQ(reports::unreadOf(list), 3);
    CHECK_EQ(std::string(reports::kindToken(reports::Kind::Crash)), std::string("crash"));
    CHECK_EQ(std::string(reports::kindToken(reports::Kind::None)), std::string(""));
    CHECK(reports::statusOf("closed") == reports::Status::Closed);
    CHECK(reports::statusOf("whatever") == reports::Status::New);
}

TEST(reports_read_the_conversation) {
    reports::Conversation c;
    std::string error;
    CHECK(reports::parseConversation(kConversation, c, error));
    CHECK_EQ(c.code, std::string("R-7K3F2A"));
    CHECK(c.kind == reports::Kind::Slow);
    CHECK(c.status == reports::Status::Fixed);
    CHECK_EQ(c.fixedIn, std::string("2.13.1"));
    CHECK_EQ(c.version, std::string("2.13.0"));
    CHECK_EQ(c.messages.size(), size_t(3));
    CHECK(!c.messages[0].fromDev);
    CHECK_EQ(c.messages[0].text, std::string("Гальмує\nв \"Пронтері\""));  // quotes and the new line whole
    CHECK(!c.messages[0].hasStatus);
    CHECK_EQ(c.messages[0].shots.size(), size_t(2));
    CHECK_EQ(c.messages[0].shots[1], std::string("m4-uaro-2.jpg"));
    CHECK(c.messages[1].fromDev);
    CHECK_EQ(c.messages[1].text, std::string("Дякуємо \xF0\x9F\x99\x82"));  // the emoji whole
    CHECK(c.messages[1].hasStatus && c.messages[1].status == reports::Status::Fixed);
    CHECK(c.messages[2].shots.empty());  // a name that is not a plain file name is no shot
}

TEST(reports_refuse_garbage) {
    std::vector<reports::Summary> list;
    reports::Conversation c;
    std::string error;
    CHECK(!reports::parseList("<html>502</html>", list, error));
    CHECK(!error.empty());
    CHECK(!reports::parseList("{\"message\":\"Too Many Attempts.\"}", list, error));
    CHECK(!reports::parseConversation("{\"reports\":[]}", c, error));
    CHECK(!reports::parseConversation("[1,2]", c, error));
}

TEST(reports_confirm_a_fix_once_the_version_is_here) {
    reports::Conversation c;
    c.status = reports::Status::Fixed;
    c.fixedIn = "2.13.1";
    CHECK(!reports::canConfirm(c, "2.13.0"));
    CHECK(reports::canConfirm(c, "2.13.1"));
    CHECK(reports::canConfirm(c, "2.14.0 build 2026-10-30 12:00"));
    c.status = reports::Status::Fixing;
    CHECK(!reports::canConfirm(c, "2.14.0"));
    c.status = reports::Status::Fixed;
    c.fixedIn.clear();
    CHECK(!reports::canConfirm(c, "2.14.0"));
}

TEST(reports_show_the_sites_time_in_the_players_clock) {
    reports::Civil utc;
    CHECK(reports::utcOf("2026-10-22T18:57:32+00:00", utc));
    CHECK(utc.year == 2026 && utc.month == 10 && utc.day == 22 && utc.hour == 18 && utc.minute == 57);
    CHECK(reports::utcOf("2026-01-01T01:30:00+03:00", utc));  // back over the new year
    CHECK(utc.year == 2025 && utc.month == 12 && utc.day == 31 && utc.hour == 22 && utc.minute == 30);
    CHECK(reports::utcOf("2024-02-28T23:00:00-02:00", utc));  // on into a leap day
    CHECK(utc.month == 2 && utc.day == 29 && utc.hour == 1);
    CHECK(!reports::utcOf("", utc));
    CHECK(!reports::utcOf("yesterday", utc));
    reports::Civil local{2026, 10, 22, 21, 7, 0};
    CHECK_EQ(reports::shown(local), std::string("22.10.2026 21:07"));
}

// 3.0.5-beta.6: «Тест графіки»'s reports listed in «Звернення» as such
TEST(reports_know_the_graphics_tests_kind) {
    CHECK_EQ(std::string(reports::kindToken(reports::Kind::Test)), std::string("test"));
    std::vector<reports::Summary> list;
    std::string error;
    CHECK(reports::parseList("{\"reports\":[{\"code\":\"R-7K3F2A\",\"kind\":\"test\",\"reason\":\"test\","
                             "\"status\":\"new\",\"title\":\"Тест графіки\"}]}",
                             list, error));
    CHECK_EQ(int(list.size()), 1);
    CHECK(list[0].kind == reports::Kind::Test);
}
