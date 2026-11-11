// 3.0.17-beta.2 (the concept «простой старт»: three choices in place of hundreds of switches): «Картинка»,
// «Інтерфейс», «Текстури» - each a set of uaro-graphics.ini's keys, matched against the player's settings
#include <set>
#include <string>

#include "core/config.hpp"
#include "core/settings/choice_presets.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

TEST(choice_presets_choices_own_their_keys) {
    std::set<std::string> seen;
    for (int c = 0; c < presets::kChoices; ++c)
        for (int v = 0; v < presets::variants(presets::Choice(c)); ++v)
            for (const auto& [key, token] : presets::keysOf(presets::Choice(c), v)) {
                const IniSetting* s = findSetting(key);
                CHECK(s != nullptr);  // every key a row of the schema
                if (s) CHECK(validToken(*s, token));
                if (v == 0) {
                    CHECK(!seen.count(key));  // no key in two choices
                    seen.insert(key);
                }
            }
}

TEST(choice_presets_match_the_settings) {
    const Config fresh = parseConfig("[effects]\npreset = auto\n");  // as dist/uaro-graphics.ini
    CHECK_EQ(presets::match(presets::Choice::Picture, fresh).variant, int(presets::Picture::Auto));
    CHECK_EQ(presets::match(presets::Choice::Picture, fresh).changed, 0);
    CHECK_EQ(presets::match(presets::Choice::Interface, fresh).variant, int(presets::Ui::New));
    CHECK_EQ(presets::match(presets::Choice::Interface, fresh).changed, 0);
    const Config remaster = parseConfig("[effects]\npreset = strong\ntier = remaster\n");
    CHECK_EQ(presets::match(presets::Choice::Picture, remaster).variant, int(presets::Picture::Remaster));
    CHECK_EQ(presets::match(presets::Choice::Picture, remaster).changed, 0);
    // one window turned off by hand: still «Новий», one changed
    const Config oneOff = parseConfig("[interface]\nminimap = 1\nchar_select = 0\n");
    CHECK_EQ(presets::match(presets::Choice::Interface, oneOff).variant, int(presets::Ui::New));
    CHECK_EQ(presets::match(presets::Choice::Interface, oneOff).changed, 1);
    // the classic interface with a part on by hand: classic, changed
    const Config classic = parseConfig("[interface]\nminimap = 0\nhotbar = 1\n");
    CHECK_EQ(presets::match(presets::Choice::Interface, classic).variant, int(presets::Ui::Classic));
    CHECK(presets::match(presets::Choice::Interface, classic).changed >= 1);
    const Config original = parseConfig("[ai]\nenabled = 0\n");
    CHECK_EQ(presets::match(presets::Choice::Textures, original).variant, int(presets::Textures::Original));
}

TEST(choice_presets_names) {
    CHECK(presets::headOf(presets::Choice::Interface) == Text::LookUi);
    CHECK(presets::nameOf(presets::Choice::Picture, int(presets::Picture::Remaster)) == Text::LookRemaster);
    CHECK(presets::nameOf(presets::Choice::Textures, int(presets::Textures::Original)) == Text::LookTexOriginal);
}

// 3.0.17-beta.9 (the owner: the card checked «без этого всего» - no recorded scenes): the picture and the textures
// by the card's level (as the game judged its drawing, else by its memory) and its memory
TEST(choice_presets_suggest_by_the_card) {
    const presets::Suggestion strong = presets::suggest(3, 12288, false);
    CHECK_EQ(strong.picture, int(presets::Picture::Remaster));
    CHECK_EQ(strong.textures, int(presets::Textures::Hd));
    const presets::Suggestion middle = presets::suggest(2, 6144, false);
    CHECK_EQ(middle.picture, int(presets::Picture::Better));
    CHECK_EQ(middle.textures, int(presets::Textures::Hd));
    const presets::Suggestion weak = presets::suggest(1, 2048, false);  // a GTX 1050 class card
    CHECK_EQ(weak.picture, int(presets::Picture::Auto));
    CHECK_EQ(weak.textures, int(presets::Textures::HdLight));
    const presets::Suggestion laptop = presets::suggest(1, 512, true);  // integrated: its memory is the computer's
    CHECK_EQ(laptop.textures, int(presets::Textures::HdLight));
    CHECK_EQ(presets::suggest(1, 1024, false).textures, int(presets::Textures::Original));
}

// 3.0.17-beta.10 (the concept: the first start's cards): a kind of player - the three choices at once; the new look
// takes the card's picture and textures (suggest), the old ones their own
TEST(choice_presets_personas) {
    const presets::Suggestion card{int(presets::Picture::Better), int(presets::Textures::Hd)};
    const presets::Persona all = presets::persona(presets::Kind::AllNew, card);
    CHECK(all.picture == card.picture && all.ui == int(presets::Ui::New) && all.textures == card.textures);
    const presets::Persona oldUi = presets::persona(presets::Kind::NewLookOldUi, card);
    CHECK(oldUi.ui == int(presets::Ui::Classic) && oldUi.picture == card.picture);
    const presets::Persona weak = presets::persona(presets::Kind::WeakPc, card);
    CHECK(weak.picture == int(presets::Picture::Auto) && weak.ui == int(presets::Ui::NewScreen) &&
          weak.textures == int(presets::Textures::HdLight));
    const presets::Persona classic = presets::persona(presets::Kind::Classic, card);
    CHECK(classic.picture == int(presets::Picture::Game) && classic.ui == int(presets::Ui::Classic) &&
          classic.textures == int(presets::Textures::Original));
}
