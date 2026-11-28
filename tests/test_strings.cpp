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

// 3.0.2-beta.6: the newest texts stand where their names are - three were put in before an older one in beta.4 and
// every text after it shifted by one (the Main page's keys heading was their list)
TEST(strings_newest_texts_in_their_places) {
    const auto en = [](Text t) { return std::string(tr(Lang::En, t)); };
    CHECK_EQ(en(Text::RoextWindowX), std::string("The window's place: X"));
    CHECK(en(Text::RoextWindowPlaceHint).rfind("Where the game's window", 0) == 0);
    CHECK_EQ(en(Text::MainKeys), std::string("Keys in the game"));
    // 3.0.21-beta.7: «Settings» (Alt+O) first - the console is the laboratory
    CHECK(en(Text::MainKeysList).rfind("Alt+O - «Settings»", 0) == 0);
    CHECK(en(Text::MainKeysList).find("\nCtrl+~ (the key left of 1) - the console: trials") != std::string::npos);
    CHECK(en(Text::PlayKeysLine).rfind("In the game: Alt+O the settings", 0) == 0);
    CHECK_EQ(en(Text::ServiceUpdates), std::string("The client's graphics updates"));
    CHECK(en(Text::UpdatesSome).rfind("Archives in the updates folder", 0) == 0);
    CHECK_EQ(en(Text::ViewHelp), std::string("Help"));
    CHECK(en(Text::HelpEffectsMore).rfind("Each group holds its effects", 0) == 0);
}

// 3.0.11-beta.58 (the autopilot): the ladder's sets by their level - the driver's notes, the Effects page's «Авто: {1}»
// and «Гра»'s line name them alike
TEST(effects_set_text_names_the_ladder) {
    CHECK(effectsSetText(0) == Text::TestSetOff);
    CHECK(effectsSetText(1) == Text::FxPresetWeak);
    CHECK(effectsSetText(2) == Text::FxPresetMedium);
    CHECK(effectsSetText(3) == Text::FxPresetStrong);
    CHECK(effectsSetText(-1) == Text::TestSetOff);
    CHECK(effectsSetText(9) == Text::FxPresetStrong);
    CHECK(std::string(tr(Lang::Uk, effectsSetText(0))) == "Без ефектів");
    CHECK(std::string(tr(Lang::Uk, effectsSetText(3))) == "Високі");
}

// 3.0.22-beta.40 (the owner's login screen over the MCP: «281 гравців онлайн», «281 игроков онлайн»): a number's word
// in its form - {N|one|few|many} for Ukrainian and Russian, {N|one|other} for English
TEST(strings_fmt_number_forms) {
    using uagfx::settings::fmt;
    const std::string uk = "{1} {1|гравець|гравці|гравців} онлайн";
    CHECK_EQ(fmt(uk, {"281"}), std::string("281 гравець онлайн"));
    CHECK_EQ(fmt(uk, {"22"}), std::string("22 гравці онлайн"));
    CHECK_EQ(fmt(uk, {"25"}), std::string("25 гравців онлайн"));
    CHECK_EQ(fmt(uk, {"11"}), std::string("11 гравців онлайн"));
    CHECK_EQ(fmt(uk, {"112"}), std::string("112 гравців онлайн"));
    CHECK_EQ(fmt(uk, {"101"}), std::string("101 гравець онлайн"));
    CHECK_EQ(fmt(uk, {"0"}), std::string("0 гравців онлайн"));
    const std::string en = "{1} {1|player|players} online";
    CHECK_EQ(fmt(en, {"1"}), std::string("1 player online"));
    CHECK_EQ(fmt(en, {"281"}), std::string("281 players online"));
    CHECK_EQ(fmt(en, {"1001"}), std::string("1001 players online"));
    CHECK_EQ(fmt(uk, {"12345678901234567891"}), std::string("12345678901234567891 гравець онлайн"));  // long
    CHECK_EQ(fmt("{1|a|b|c}", {"many"}), std::string("c"));  // not a number: the last form
    CHECK_EQ(fmt("{1} · {2}", {"x", "y"}), std::string("x · y"));  // as before
}

// 3.0.24-beta.41 (the laptop's effects run: «Тіні від факелів» on in «Класика» showed nothing - the torches' shadows
// are drawn in the remaster mode alone, on both renderers, its hint silent; so the stamp behind characters on
// Direct3D 11): a switch that works in one mode only says which
TEST(strings_remaster_only_switches_say_so) {
    for (const Lang l : {Lang::En, Lang::Uk, Lang::Ru}) {
        const std::string torch = tr(l, Text::FxTorchShadowsHint), stamp = tr(l, Text::FxBodyStampHint);
        CHECK(torch.find("Remaster") != std::string::npos || torch.find("Ремастер") != std::string::npos);
        CHECK(stamp.find("Direct3D 11") != std::string::npos);
        CHECK(stamp.find("Remaster") != std::string::npos || stamp.find("Ремастер") != std::string::npos);
    }
}
