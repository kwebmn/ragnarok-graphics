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

TEST(choice_presets_apply_writes_only_its_keys) {
    const std::string ini = "[effects]\ngrass = 0\n[interface]\nminimap = 1\n";
    const std::string out = presets::apply(ini, presets::Choice::Interface, int(presets::Ui::Classic));
    const Config c = parseConfig(out);
    CHECK(!c.newMinimap);
    CHECK(!c.hotbar && !c.itemsWindow);
    CHECK(out.find("grass = 0") != std::string::npos);  // the other choices' keys stay
    CHECK_EQ(presets::match(presets::Choice::Interface, c).changed, 0);
}
