#include "core/settings/search.hpp"

#include <algorithm>

#include "core/settings/game_settings.hpp"
#include "core/settings/old_names.hpp"

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

// .301: a word's stem - as many code points as are kept
std::string stem(const std::string& w) {
    std::vector<size_t> starts;
    for (size_t i = 0; i < w.size();) {
        starts.push_back(i);
        next(w, i);
    }
    const size_t n = starts.size();
    const size_t keep = n >= 5 ? std::max<size_t>(4, n - 2) : n == 4 ? 3 : n;
    return keep < n ? w.substr(0, starts[keep]) : w;
}

// .301: a word's synonyms - the folded beginnings that call them, what is looked for then (UTF-8, folded)
struct Synonym {
    std::vector<std::string> calls, finds;
};
const std::vector<Synonym>& synonyms() {
    static const std::vector<Synonym> list{
        {{"шрифт", "font", "букв"}, {"розмір тексту", "размер текста", "text size", "шрифт"}},
        {{"плавн", "smooth"}, {"згладж", "сглаж", "smoothing"}},
        {{"лаг", "фриз", "тормоз", "гальм", "lag", "stutter", "подвис", "зависа"},
         {"кадр", "fps", "з'єдн", "соедин", "connection", "затримк", "задержк"}},
        {{"мыш", "миш", "mouse"}, {"курсор", "cursor", "миш", "мыш"}},
        {{"окон", "вікон", "windowed"}, {"вікн", "окн", "window"}},
        {{"разреш", "роздільн", "resolution"}, {"роздільн", "разреш", "resolution", "масштаб", "scale"}},
        {{"пинг", "пінг", "ping"}, {"пінг", "пинг", "ping", "з'єдн", "соедин", "connection"}},
        {{"фпс"}, {"fps", "кадр"}},
        {{"джойст", "геймп", "контрол", "gamepad", "joystick", "xbox", "dualsense", "dualshock"},
         {"геймпад", "контролер", "контроллер", "gamepad", "controller"}},
        {{"тормоз", "медлен", "повіль", "slow"}, {"швидк", "быстр", "fast", "слабк", "слаб", "weak", "батаре"}},
    };
    return list;
}

// .301: the alternatives a query word is looked for by: its stem, and its synonyms' words
std::vector<std::string> alternatives(const std::string& word) {
    std::vector<std::string> alts{stem(word)};
    for (const Synonym& s : synonyms())
        for (const std::string& c : s.calls)
            if (word.compare(0, c.size(), c) == 0 || (c.compare(0, word.size(), word) == 0 && letters(word) >= 3)) {
                alts.insert(alts.end(), s.finds.begin(), s.finds.end());
                break;
            }
    return alts;
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

bool queryMatches(const std::string& text, const std::string& query) {
    const std::vector<std::string> list = words(foldCase(query));
    if (list.empty()) return false;
    const std::string folded = foldCase(text);
    return std::all_of(list.begin(), list.end(), [&](const std::string& w) {
        const std::vector<std::string> alts = alternatives(w);
        return std::any_of(alts.begin(), alts.end(),
                           [&](const std::string& a) { return !a.empty() && folded.find(a) != std::string::npos; });
    });
}

namespace {
// .301: a row's name in every language - its label and its name before (a match there ranks first)
std::string allNames(const IniSetting& s) {
    std::string text;
    for (const Lang l : {Lang::Uk, Lang::Ru, Lang::En}) {
        text += std::string(tr(l, s.label)) + "\n";
        if (const char* old = oldName(sectionKey(s), l)) text += std::string(old) + "\n";
    }
    return text;
}
// .301: a row's words in every language - its label, its hint, its name before
std::string allWords(const IniSetting& s) {
    std::string text;
    for (const Lang l : {Lang::Uk, Lang::Ru, Lang::En}) {
        text += std::string(tr(l, s.label)) + "\n" + tr(l, s.hint) + "\n";
        if (const char* old = oldName(sectionKey(s), l)) text += std::string(old) + "\n";
    }
    return text;
}
}  // namespace

bool settingMatches(const IniSetting& s, const std::string& query, Lang) {
    return queryMatches(allWords(s), query);  // .301: the console's letters as the app's search
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
        // .301: the hand-built pages' parts
        {Text::ScreenWindow, {View::Screen, 0}, Mode::Detailed, "віконний оконный режим window mode"},
        {Text::ScreenFullscreen, {View::Screen, 0}, Mode::Detailed, "повноекранний полноэкранный fullscreen режим"},
        {Text::ScreenWindowSize, {View::Screen, 0}, Mode::Detailed, "розмір вікна размер окна роздільність"},
        {Text::ScreenMonitor, {View::Screen, 0}, Mode::Detailed, "монітор монитор екран экран display"},
        {Text::PadType, {View::Pad, 0}, Mode::Simple, "геймпад джойстик xbox dualsense dualshock playstation"},
        {Text::PadRumble, {View::Pad, 0}, Mode::Simple, "вібрація вибрация rumble"},
        {Text::PadLightbar, {View::Pad, 0}, Mode::Simple, "підсвітка подсветка lightbar"},
        {Text::PadButtons, {View::Pad, 0}, Mode::Simple, "кнопки геймпада buttons"},
        {Text::PadChatWheel, {View::Pad, 0}, Mode::Simple, ""},
        {Text::PlayTestButton, {View::Play, 0}, Mode::Simple, "тест графіки тест графики test"},
        {Text::UpdateNow, {View::Play, 0}, Mode::Simple, "оновлення обновление update"},
    };
    return list;
}

std::vector<Hit> findSettings(const std::string& query, Lang l, Mode m) {
    (void)l;  // .301: every language's words
    if (letters(query) < 2) return {};
    const Mode most = m == Mode::Developer ? Mode::Developer : Mode::Detailed;  // .301: the Detailed's from Simple
    const std::vector<View> list = viewsOf(most);
    struct Ranked {
        size_t view;
        Hit hit;
    };
    std::vector<Ranked> found;
    // .301: the mode's own first, a match in the name before one in a hint alone, then the list's order
    const auto add = [&](HitKind kind, size_t index, Place place, Mode level, bool named = true) {
        const size_t at = size_t(std::find(list.begin(), list.end(), place.view) - list.begin());
        const size_t rank = (at < list.size() ? at : list.size() + size_t(place.view)) + (level > m ? 1000 : 0) +
                            (named ? 0 : 500);
        found.push_back({rank, {kind, index, place, std::max(level, m)}});
    };
    const std::vector<IniSetting>& rows = iniSettings();
    for (size_t i = 0; i < rows.size(); ++i) {
        const IniSetting& s = rows[i];
        if (levelOf(s) > most) continue;
        std::string text = allWords(s);
        if (m == Mode::Developer) text += "\n" + sectionKey(s);  // the ini's own words too
        if (!queryMatches(text, query)) continue;
        const bool main = m == Mode::Simple && mainKey(sectionKey(s));
        add(HitKind::Ini, i, main ? Place{View::Main, 0} : placeOf(s), main ? Mode::Simple : levelOf(s),
            queryMatches(allNames(s), query));
    }
    {  // the game's own key: the Detailed mode's pages
        const std::vector<RegSetting>& reg = regSettings();
        for (size_t i = 0; i < reg.size(); ++i) {
            std::string text;
            for (const Lang k : {Lang::Uk, Lang::Ru, Lang::En})
                text += std::string(tr(k, reg[i].label)) + "\n" + tr(k, reg[i].hint) + "\n";
            if (queryMatches(text, query)) add(HitKind::Reg, i, placeOf(reg[i].page), Mode::Detailed);
        }
    }
    const std::vector<Action>& acts = actions();
    for (size_t i = 0; i < acts.size(); ++i) {
        if (acts[i].level > most) continue;
        std::string text = std::string(acts[i].words) + "\n";
        for (const Lang k : {Lang::Uk, Lang::Ru, Lang::En}) text += std::string(tr(k, acts[i].label)) + "\n";
        if (queryMatches(text, query)) add(HitKind::Action, i, acts[i].place, acts[i].level);
    }
    std::stable_sort(found.begin(), found.end(), [](const Ranked& a, const Ranked& b) { return a.view < b.view; });
    std::vector<Hit> hits;
    for (const Ranked& r : found) {
        if (hits.size() == kMostHits) break;
        hits.push_back(r.hit);
    }
    return hits;
}

}  // namespace uagfx::settings
