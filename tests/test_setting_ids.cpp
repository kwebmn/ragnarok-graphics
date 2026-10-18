#include "app/setting_ids.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

TEST(settings_page_every_row_has_ids_of_its_own) {
    // 2.4.2: the rows' ids ran from 1100 by four, and from row 98 on (the Frames page, the Sound page's own part) into
    // the page's other ids - those rows got no commands: their boxes and lists changed nothing
    const std::vector<IniSetting>& all = iniSettings();
    for (size_t i = 0; i < all.size(); ++i) {
        const int id = app::rowId(i);
        if (app::settingOfId(id) != &all[i]) std::printf("  no command: %s.%s\n", all[i].section, all[i].key);
        CHECK(app::settingOfId(id) == &all[i]);
        CHECK(app::settingOfId(id + 1) == nullptr);  // its number's, its unit's
        CHECK(id + 3 < 65536);
        for (int other : {app::kForgetLogin, app::kAuto, app::kGpu}) CHECK(other < id || other > id + 3);
        CHECK(id >= app::kReg + 4 * int(regSettings().size()) || id + 3 < app::kReg);
    }
    CHECK(app::settingOfId(app::kForgetLogin) == nullptr);
}
