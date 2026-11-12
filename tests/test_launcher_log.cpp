// 3.0.18-beta.195 (docs/superpowers/plans/2026-11-12-launcher.md Task 8): the launcher's «Журнал змін» - the versions
// by day on the left («Сьогодні», «Вчора», dates), the filters, the chosen version's points on the right
#include <string>
#include <vector>

#include "core/launcher_layout.hpp"
#include "core/launcher_log.hpp"
#include "core/settings/changelog.hpp"
#include "test.hpp"

using namespace uagfx;
using launcherlog::Filter;
using settings::changelog::Version;

namespace {

Version version(const char* name, const char* date, bool beta) {
    Version v;
    v.version = name;
    v.date = date;
    v.beta = beta;
    v.points.push_back({std::string("a point of ") + name, false});
    return v;
}

std::vector<Version> sample() {
    return {version("3.0.18-beta.193", "2026-11-12", true), version("3.0.18-beta.192", "2026-11-12", true),
            version("3.0.17", "2026-11-11", false), version("3.0.17-beta.27", "2026-11-09", true),
            version("3.0.16", "2025-12-30", false), version("3.0.15", "", false)};
}

}  // namespace

TEST(launcher_log_days) {
    using settings::Lang;
    CHECK_EQ(launcherlog::dayWords("2026-11-12", "2026-11-12", Lang::Uk), std::string("Сьогодні"));
    CHECK_EQ(launcherlog::dayWords("2026-11-11", "2026-11-12", Lang::Uk), std::string("Вчора"));
    CHECK_EQ(launcherlog::dayWords("2026-10-31", "2026-11-01", Lang::Uk), std::string("Вчора"));  // over a month
    CHECK_EQ(launcherlog::dayWords("2026-11-09", "2026-11-12", Lang::Uk), std::string("9 листопада"));
    CHECK_EQ(launcherlog::dayWords("2025-12-30", "2026-11-12", Lang::Uk), std::string("30 грудня 2025"));
    CHECK_EQ(launcherlog::dayWords("2026-11-09", "2026-11-12", Lang::Ru), std::string("9 ноября"));
    CHECK_EQ(launcherlog::dayWords("2026-11-09", "2026-11-12", Lang::En), std::string("9 November"));
    CHECK_EQ(launcherlog::dayWords("", "2026-11-12", Lang::Uk), std::string());
    CHECK_EQ(launcherlog::dayWords("soon", "2026-11-12", Lang::Uk), std::string());
    // the list: a day's words over its first version only
    const std::vector<Version> v = sample();
    const launcherview::Layout L = launcherview::layoutFor(1280, 720, 1.0f, 3, false);
    const launcherlog::Screen s = launcherlog::layoutFor(L, v, Filter::All, "", "2026-11-12", 0, Lang::Uk);
    CHECK_EQ(s.rows.size(), v.size());
    CHECK_EQ(s.rows[0].day, std::string("Сьогодні"));
    CHECK_EQ(s.rows[1].day, std::string());
    CHECK_EQ(s.rows[2].day, std::string("Вчора"));
    CHECK_EQ(s.rows[3].day, std::string("9 листопада"));
    CHECK_EQ(s.rows[4].day, std::string("30 грудня 2025"));
    CHECK_EQ(s.rows[5].day, std::string());  // no date: under the one before
    for (size_t i = 0; i < s.rows.size(); ++i) {
        CHECK_EQ(s.rows[i].version, int(i));
        if (i) CHECK(s.rows[i].box.y >= s.rows[i - 1].box.y + s.rows[i - 1].box.h);
        CHECK(s.rows[i].box.x >= s.list.x && s.rows[i].box.x + s.rows[i].box.w <= s.list.x + s.list.w + 0.01f);
    }
    CHECK(s.page.x >= s.list.x + s.list.w);
    CHECK(s.contentH >= s.rows.back().box.y + s.rows.back().box.h - s.list.y);
    // scrolled: the rows up by it; a row's centre hit
    const launcherlog::Screen down = launcherlog::layoutFor(L, v, Filter::All, "", "2026-11-12", 40, Lang::Uk);
    CHECK(down.rows[0].box.y < s.rows[0].box.y - 39);
    const launcherview::Box& b = s.rows[2].box;
    CHECK_EQ(launcherlog::rowAt(s, b.x + b.w / 2, b.y + b.h / 2), 2);
    CHECK_EQ(launcherlog::rowAt(s, 2, 2), -1);
}

TEST(launcher_log_filters) {
    const std::vector<Version> v = sample();
    const std::vector<int> releases = launcherlog::filtered(v, Filter::Releases, "");
    CHECK(releases == std::vector<int>({2, 4, 5}));
    // «Нове для мене»: the versions newer than the one last seen
    CHECK(launcherlog::filtered(v, Filter::New, "3.0.17") == std::vector<int>({0, 1}));
    CHECK(launcherlog::filtered(v, Filter::New, "").empty());  // nothing seen yet: nothing «new»
    CHECK_EQ(launcherlog::filtered(v, Filter::All, "3.0.17").size(), v.size());
    const launcherview::Layout L = launcherview::layoutFor(1280, 720, 1.0f, 3, false);
    const launcherlog::Screen s =
        launcherlog::layoutFor(L, v, Filter::Releases, "", "2026-11-12", 0, settings::Lang::Uk);
    CHECK_EQ(s.rows.size(), size_t(3));
    CHECK_EQ(s.rows[0].version, 2);
    CHECK_EQ(s.rows[0].day, std::string("Вчора"));
}
