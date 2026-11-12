#include "core/launcher_log.hpp"

#include <algorithm>

namespace uagfx::launcherlog {
namespace {

// At scale 1 (the design 1280 x 720)
constexpr float kLeft = 22, kRight = 26, kBottom = 26, kTop = 80, kBackW = 132, kHeadH = 36, kListW = 320;
constexpr float kListTop = 132, kGap = 20, kRowH = 54, kRowGap = 6, kDayH = 30;
constexpr float kChipAll = 90, kChipReleases = 150, kChipNew = 190, kChipGap = 8;

struct Date {
    int y = 0, m = 0, d = 0;
};

bool parseDate(const std::string& s, Date& out) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') return false;
    for (size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u})
        if (s[i] < '0' || s[i] > '9') return false;
    out.y = std::stoi(s.substr(0, 4));
    out.m = std::stoi(s.substr(5, 2));
    out.d = std::stoi(s.substr(8, 2));
    return out.m >= 1 && out.m <= 12 && out.d >= 1 && out.d <= 31;
}

// The day's number since 1970-01-01 (H. Hinnant's days_from_civil)
long dayNumber(const Date& t) {
    const int y = t.y - (t.m <= 2 ? 1 : 0);
    const long era = (y >= 0 ? y : y - 399) / 400;
    const long yoe = y - era * 400;
    const long doy = (153L * (t.m + (t.m > 2 ? -3 : 9)) + 2) / 5 + t.d - 1;
    const long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

std::string monthOf(int m, settings::Lang l) {
    const std::string all = settings::tr(l, settings::Text::LauncherMonths);  // «січня|лютого|...»
    size_t from = 0;
    for (int i = 1; i < m; ++i) {
        from = all.find('|', from);
        if (from == std::string::npos) return "";
        ++from;
    }
    const size_t to = all.find('|', from);
    return all.substr(from, to == std::string::npos ? std::string::npos : to - from);
}

}  // namespace

std::string dayWords(const std::string& date, const std::string& today, settings::Lang l) {
    Date d, now;
    if (!parseDate(date, d)) return "";
    if (parseDate(today, now)) {
        const long apart = dayNumber(now) - dayNumber(d);
        if (apart == 0) return settings::tr(l, settings::Text::LauncherToday);
        if (apart == 1) return settings::tr(l, settings::Text::LauncherYesterday);
    }
    std::string words = std::to_string(d.d) + " " + monthOf(d.m, l);
    if (!parseDate(today, now) || now.y != d.y) words += " " + std::to_string(d.y);
    return words;
}

std::vector<int> filtered(const std::vector<settings::changelog::Version>& v, Filter f, const std::string& seen) {
    std::vector<int> out;
    const size_t fresh = f == Filter::New ? settings::changelog::newerThan(v, seen) : 0;
    for (size_t i = 0; i < v.size(); ++i) {
        if (f == Filter::Releases && v[i].beta) continue;
        if (f == Filter::New && i >= fresh) break;
        out.push_back(int(i));
    }
    return out;
}

Screen layoutFor(const launcherview::Layout& L, const std::vector<settings::changelog::Version>& v, Filter f,
                 const std::string& seen, const std::string& today, float scroll, settings::Lang l) {
    Screen s;
    const float k = L.s > 0 ? L.s : 1.0f;
    s.s = k;
    const float W = float(std::max(L.w, 1)), H = float(std::max(L.h, 1));
    // 3.0.18-beta.203: under the logo (taller than the bar), the list as far below the header as before
    const float top = std::max(kTop * k, L.logo.y + L.logo.h + 10 * k), listTop = kListTop * k + (top - kTop * k);
    s.back = Box{kLeft * k, top, kBackW * k, kHeadH * k};
    // the filters at the right of the header, the title between
    const float widths[kFilters] = {kChipAll * k, kChipReleases * k, kChipNew * k};
    float x = W - kRight * k;
    for (int i = kFilters - 1; i >= 0; --i) {
        x -= widths[i];
        s.filters[i] = Box{x, top, widths[i], kHeadH * k};
        x -= kChipGap * k;
    }
    const float titleX = s.back.x + s.back.w + 16 * k;
    s.title = Box{titleX, top, std::max(0.0f, x - titleX), kHeadH * k};
    const float listW = std::min(kListW * k, W * 0.4f);
    s.list = Box{kLeft * k, listTop, listW, std::max(0.0f, H - kBottom * k - listTop)};
    const float pageX = s.list.x + s.list.w + kGap * k;
    s.page = Box{pageX, s.list.y, std::max(0.0f, W - kRight * k - pageX), s.list.h};
    // the rows: a day's words over its first version
    float y = 0;
    std::string lastDay;
    for (int i : filtered(v, f, seen)) {
        Row r;
        r.version = i;
        const std::string day = dayWords(v[size_t(i)].date, today, l);
        if (!day.empty() && day != lastDay) {
            r.day = day;
            y += kDayH * k;
        }
        if (!day.empty()) lastDay = day;
        r.box = Box{s.list.x, s.list.y + y - scroll, s.list.w, kRowH * k};
        y += (kRowH + kRowGap) * k;
        s.rows.push_back(r);
    }
    s.contentH = y;
    return s;
}

int rowAt(const Screen& s, float x, float y) {
    if (!s.list.contains(x, y)) return -1;
    for (size_t i = 0; i < s.rows.size(); ++i)
        if (s.rows[i].box.contains(x, y)) return int(i);
    return -1;
}

}  // namespace uagfx::launcherlog
