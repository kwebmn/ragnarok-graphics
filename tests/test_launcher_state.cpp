// 2.11: the launcher «Гра» in the settings app (app/launcher) over the stand-ins of tests/fake_windows.cpp.
#include <string>

#include "app/launcher.hpp"
#include "app/model.hpp"
#include "fake_windows.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(launcher_state_from_the_site) {
    fake::Windows& w = fake::reset();
    w.web["/api/public/launcher?lang=ru"] =
        "{\"online\":152,\"peak\":165,\"news\":[{\"title\":"
        "\"\\u041d\\u043e\\u0432\\u043e\\u0441\\u0442\\u044c\",\"excerpt\":\"x\",\"date\":\"2026-10-20\","
        "\"pinned\":true,\"url\":\"https:\\/\\/uaro.kiev.ua\\/ru\\/news\\/a\"}]}";
    const app::LauncherState s = app::launcherNow(settings::Lang::Ru);
    CHECK(s.kind == app::LauncherState::Done);
    CHECK(s.info.online == 152);
    CHECK_EQ(s.info.news.size(), size_t(1));
    CHECK_EQ(s.info.news[0].title, std::string("Новость"));
}

TEST(launcher_without_the_site_still_plays) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaRO.exe")] = "MZ uaRO";
    const app::LauncherState s = app::launcherNow(settings::Lang::Uk);  // no answer
    CHECK(s.kind == app::LauncherState::Failed);
    CHECK(!s.error.empty());
    app::Model m;
    CHECK(m.launchGame(settings::Lang::Uk).empty());
    CHECK_EQ(w.launched.size(), size_t(1));
    CHECK(w.launched[0] == fake::path(L"uaRO.exe"));
    CHECK(w.marked);  // the driver does not send it back to «Гра»
}
