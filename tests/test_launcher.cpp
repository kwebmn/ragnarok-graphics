// 2.11: the launcher «Гра» - the site's answer (GET /api/public/launcher) as core/launcher reads it.
#include <string>

#include "core/launcher.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// As Laravel writes it: non-ASCII as \u escapes, slashes as "\/".
const char* const kAnswer =
    "{\"online\":124,\"peak\":165,\"news\":["
    "{\"title\":\"\\u041e\\u0441\\u0456\\u043d\\u043d\\u0456\\u0439 \\\"\\u0444\\u0435\\u0441\\u0442\\u0438\\u0432"
    "\\u0430\\u043b\\u044c\\\" \\ud83c\\udf83\",\"excerpt\":\"line one\\nline two\",\"date\":\"2026-10-20\","
    "\"pinned\":true,\"url\":\"https:\\/\\/uaro.kiev.ua\\/uk\\/news\\/autumn\"},"
    "{\"title\":\"Tech works\",\"excerpt\":\"\",\"date\":\"2026-10-18\",\"pinned\":false,"
    "\"url\":\"https:\\/\\/evil.example\\/uaro.kiev.ua\"}]}";

}  // namespace

TEST(launcher_reads_the_sites_news) {
    launcher::Info info;
    std::string error;
    CHECK(launcher::parse(kAnswer, info, error));
    CHECK(info.online == 124 && info.peak == 165);
    CHECK_EQ(info.news.size(), size_t(2));
    CHECK_EQ(info.news[0].title, std::string("Осінній \"фестиваль\" \xF0\x9F\x8E\x83"));  // the emoji whole
    CHECK_EQ(info.news[0].excerpt, std::string("line one\nline two"));
    CHECK_EQ(info.news[0].date, std::string("2026-10-20"));
    CHECK(info.news[0].pinned && !info.news[1].pinned);
    CHECK_EQ(info.news[0].url, std::string("https://uaro.kiev.ua/uk/news/autumn"));
    CHECK_EQ(info.news[1].url, std::string(""));  // another site's link is not opened
}

TEST(launcher_refuses_what_is_not_its_answer) {
    launcher::Info info;
    std::string error;
    CHECK(!launcher::parse("<html><body>502 Bad Gateway</body></html>", info, error));
    CHECK(!launcher::parse("[]", info, error));
    CHECK(!launcher::parse("{\"online\":1,\"news\":5}", info, error));
    CHECK(launcher::parse("{\"online\":3,\"peak\":4,\"news\":[]}", info, error));  // no news: still an answer
    CHECK(info.news.empty() && info.online == 3);
}

TEST(launcher_asks_in_the_players_language) {
    CHECK_EQ(launcher::langToken(settings::Lang::Uk), std::string("uk"));
    CHECK_EQ(launcher::langToken(settings::Lang::Ru), std::string("ru"));
    CHECK_EQ(launcher::langToken(settings::Lang::En), std::string("en"));
}

// 2.11: a direct start of uaRO.exe opens «Гра» instead (decision 13) - not the launcher's own start (UARO_LAUNCHER),
// not with «Запускати гру одразу», not without uaro-graphics.exe next to the game.
TEST(the_game_redirects_only_a_direct_start_without_the_box) {
    CHECK(launcher::redirect(false, false, true));
    CHECK(!launcher::redirect(true, false, true));   // «Грати», the update's restart
    CHECK(!launcher::redirect(false, true, true));   // [launcher] direct_start = 1
    CHECK(!launcher::redirect(false, false, false));  // no launcher there: the game plays
}
