#include <set>

#include "core/settings/strings.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

std::multiset<std::string> placeholders(const std::string& s) {
    std::multiset<std::string> out;
    for (size_t i = 0; i + 2 < s.size(); ++i)
        if (s[i] == '{' && s[i + 2] == '}' && s[i + 1] >= '1' && s[i + 1] <= '9') out.insert(s.substr(i, 3));
    return out;
}

}  // namespace

TEST(strings_every_text_in_three_languages) {
    for (int t = 0; t < int(Text::Count); ++t) {
        const std::string en = tr(Lang::En, Text(t));
        CHECK(!en.empty());
        for (Lang l : {Lang::Uk, Lang::Ru}) {
            const std::string other = tr(l, Text(t));
            CHECK(!other.empty());
            CHECK(placeholders(other) == placeholders(en));  // the same {N} in every language
        }
    }
}

TEST(strings_fill_placeholders) {
    CHECK(fmt("Game {1}×{2}, {1}", {"1920", "1080"}) == "Game 1920×1080, 1920");
    CHECK(fmt("{3} missing", {"a"}) == " missing");
    CHECK(fmt(tr(Lang::Ru, Text::ScreenResult), {"1920", "1080", "3360", "1890", "1,75"}) ==
          "Игра 1920×1080 → экран 3360×1890, интерфейс ×1,75");
}

TEST(strings_languages_and_the_first_one) {
    CHECK(parseLang("RU") == Lang::Ru);
    CHECK(parseLang("uk") == Lang::Uk);
    CHECK(!parseLang("de").has_value());
    CHECK(std::string(langToken(Lang::Uk)) == "uk");
    // 0.48: English until a language is chosen (it was the language of Windows); the chosen one stays.
    CHECK(startLang(std::nullopt) == Lang::En);
    CHECK(startLang(Lang::Ru) == Lang::Ru);
    CHECK(startLang(Lang::Uk) == Lang::Uk);
}
