#include "core/settings/search.hpp"

#include <algorithm>

#include "core/settings/game_settings.hpp"

namespace uagfx::settings {
namespace {

constexpr size_t kMostHits = 80;

// One code point from s at i (moved past it); a broken byte reads as itself.
uint32_t next(const std::string& s, size_t& i) {
    const unsigned char c = static_cast<unsigned char>(s[i]);
    int more = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    uint32_t cp = more == 3 ? c & 0x07u : more == 2 ? c & 0x0Fu : more == 1 ? c & 0x1Fu : c;
    ++i;
    for (; more > 0 && i < s.size() && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80; --more, ++i)
        cp = (cp << 6) | (static_cast<unsigned char>(s[i]) & 0x3Fu);
    return cp;
}

void put(std::string& out, uint32_t cp) {
    if (cp < 0x80) {
        out += char(cp);
    } else if (cp < 0x800) {
        out += char(0xC0 | (cp >> 6));
        out += char(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
        out += char(0xE0 | (cp >> 12));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    } else {
        out += char(0xF0 | (cp >> 18));
        out += char(0x80 | ((cp >> 12) & 0x3F));
        out += char(0x80 | ((cp >> 6) & 0x3F));
        out += char(0x80 | (cp & 0x3F));
    }
}

uint32_t lower(uint32_t cp) {
    if (cp >= 'A' && cp <= 'Z') return cp + 32;
    if (cp >= 0x410 && cp <= 0x42F) return cp + 0x20;  // А-Я
    if (cp >= 0x400 && cp <= 0x40F) cp += 0x50;         // Ѐ-Џ: Ё, Є, І, Ї
    if (cp == 0x490) return 0x491;                      // Ґ
    if (cp == 0x451) return 0x435;                      // ё as е
    return cp;
}

std::vector<std::string> words(const std::string& folded) {
    std::vector<std::string> list;
    std::string word;
    for (char ch : folded + " ") {
        if (ch == ' ' || ch == '\t') {
            if (!word.empty()) list.push_back(word);
            word.clear();
        } else {
            word += ch;
        }
    }
    return list;
}

size_t letters(const std::string& s) {  // code points that are not spaces
    size_t n = 0;
    for (size_t i = 0; i < s.size();)
        if (next(s, i) > ' ') ++n;
    return n;
}

}  // namespace

std::string foldCase(const std::string& utf8) {
    std::string out;
    out.reserve(utf8.size());
    for (size_t i = 0; i < utf8.size();) {
        const uint32_t cp = next(utf8, i);
        if (cp != '&') put(out, lower(cp));
    }
    return out;
}

bool textMatches(const std::string& text, const std::string& query) {
    const std::vector<std::string> list = words(foldCase(query));
    if (list.empty()) return false;
    const std::string folded = foldCase(text);
    return std::all_of(list.begin(), list.end(),
                       [&](const std::string& w) { return folded.find(w) != std::string::npos; });
}

const std::vector<Action>& actions() {
    static const std::vector<Action> list{
        {Text::PadHeading, {View::Pad, 0}, Mode::Simple},
        {Text::GameQualityHeading, {View::Graphics, 0}, Mode::Detailed},
        {Text::ScreenShow, {View::Screen, 0}, Mode::Detailed},
        {Text::ScreenUi, {View::Screen, 0}, Mode::Detailed},
        {Text::AiGroupsHeading, {View::Ai, 1}, Mode::Detailed},
        {Text::AiPreviewHeading, {View::Ai, 1}, Mode::Developer},
        {Text::PackMakeHeading, {View::Ai, 2}, Mode::Detailed},
        {Text::PackMapHeading, {View::Ai, 2}, Mode::Developer},
        {Text::PackCompact, {View::Ai, 2}, Mode::Developer},
        {Text::PageAiCache, {View::Ai, 3}, Mode::Detailed},
        {Text::ServicePresets, {View::Service, 0}, Mode::Detailed},
        {Text::ServiceProfiles, {View::Service, 0}, Mode::Detailed},
        {Text::ServiceBackups, {View::Service, 0}, Mode::Detailed},
        {Text::UpdateCheck, {View::Service, 0}, Mode::Detailed},
        {Text::ReportHeading, {View::Service, 0}, Mode::Detailed},
        {Text::ServicePalettes, {View::Service, 0}, Mode::Developer},
    };
    return list;
}

std::vector<Hit> findSettings(const std::string& query, Lang l, Mode m) {
    if (letters(query) < 2) return {};
    const std::vector<View> list = viewsOf(m);
    struct Ranked {
        size_t view;
        Hit hit;
    };
    std::vector<Ranked> found;
    const auto add = [&](HitKind kind, size_t index, Place place) {
        const size_t at = size_t(std::find(list.begin(), list.end(), place.view) - list.begin());
        found.push_back({at < list.size() ? at : list.size() + size_t(place.view), {kind, index, place}});
    };
    const std::vector<IniSetting>& rows = iniSettings();
    for (size_t i = 0; i < rows.size(); ++i) {
        const IniSetting& s = rows[i];
        if (levelOf(s) > m) continue;
        std::string text = std::string(tr(l, s.label)) + "\n" + tr(l, s.hint);
        if (m == Mode::Developer) text += "\n" + sectionKey(s);  // the ini's own words too
        if (!textMatches(text, query)) continue;
        add(HitKind::Ini, i, m == Mode::Simple && mainKey(sectionKey(s)) ? Place{View::Main, 0} : placeOf(s));
    }
    if (m != Mode::Simple) {  // the game's own key: the Detailed mode's pages
        const std::vector<RegSetting>& reg = regSettings();
        for (size_t i = 0; i < reg.size(); ++i)
            if (textMatches(std::string(tr(l, reg[i].label)) + "\n" + tr(l, reg[i].hint), query))
                add(HitKind::Reg, i, placeOf(reg[i].page));
    }
    const std::vector<Action>& acts = actions();
    for (size_t i = 0; i < acts.size(); ++i)
        if (acts[i].level <= m && textMatches(tr(l, acts[i].label), query)) add(HitKind::Action, i, acts[i].place);
    std::stable_sort(found.begin(), found.end(), [](const Ranked& a, const Ranked& b) { return a.view < b.view; });
    std::vector<Hit> hits;
    for (const Ranked& r : found) {
        if (hits.size() == kMostHits) break;
        hits.push_back(r.hit);
    }
    return hits;
}

}  // namespace uagfx::settings
