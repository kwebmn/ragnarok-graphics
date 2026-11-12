#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/launcher_layout.hpp"
#include "core/settings/changelog.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::launcherlog {

// 3.0.18-beta.195 (docs/superpowers/specs/2026-11-12-launcher-design.md «Журнал змін»; the owner: «хотелось бы
// ченжлог... что там происходит»): the launcher's changelog screen over its darkened art - «← До гри», the filters
// «Усе» / «Лише релізи» / «Нове для мене · N»; the versions by day on the left («Сьогодні», «Вчора», dates), the
// chosen version's points on the right (the view lays them out: their words are measured).

using launcherview::Box;

enum class Filter : uint8_t { All, Releases, New };
constexpr int kFilters = 3;

struct Row {
    int version = -1;  // its index in the versions given
    Box box;
    std::string day;  // the day's words over it, "" when the row before is of its day (or it has no date)
};
struct Screen {
    Box back, title, filters[kFilters], list, page;
    std::vector<Row> rows;
    float contentH = 0;  // the list's days and rows: how far it scrolls
    float s = 1;
};

// "2026-11-09" as the player reads it beside today ("2026-11-12"): «Сьогодні», «Вчора», «9 листопада», a year when it
// is another's («30 грудня 2025»); "" for no date
std::string dayWords(const std::string& date, const std::string& today, settings::Lang l);
// The versions a filter shows, by their index: all, the releases, those newer than the one seen (none when none seen)
std::vector<int> filtered(const std::vector<settings::changelog::Version>& v, Filter f, const std::string& seen);
// scroll: the list's, in pixels from its top
Screen layoutFor(const launcherview::Layout& L, const std::vector<settings::changelog::Version>& v, Filter f,
                 const std::string& seen, const std::string& today, float scroll, settings::Lang l);
int rowAt(const Screen& s, float x, float y);  // the row's place in s.rows, -1 none (outside the list too)

}  // namespace uagfx::launcherlog
