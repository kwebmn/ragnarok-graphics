#include <string>

#include "core/config.hpp"
#include "core/live_config.hpp"
#include "core/notify.hpp"
#include "core/settings/schema.hpp"
#include "core/settings/strings.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.4-beta.3 (the user's idea 2): a game window behind others tells of whispers, invitations, trades, friends and
// the character's death - its taskbar button flashes and Windows shows a notification ([notify]).
TEST(notify_settings_on_unless_turned_off) {
    const NotifySettings d = parseConfig("").notify;
    CHECK(d.on && d.whisper && d.party && d.trade && d.guild && d.friendAsk && d.death);
    const NotifySettings s =
        parseConfig("[notify]\non = 0\nwhisper = 0\nparty = 0\ntrade = 0\nguild = 0\nfriend = 0\ndeath = 0\n").notify;
    CHECK(!s.on && !s.whisper && !s.party && !s.trade && !s.guild && !s.friendAsk && !s.death);
    for (const char* k : {"notify.on", "notify.whisper", "notify.party", "notify.trade", "notify.guild",
                          "notify.friend", "notify.death"}) {
        const settings::IniSetting* row = settings::findSetting(k);
        CHECK(row != nullptr);
        if (!row) continue;
        CHECK(row->page == settings::Page::Game && row->control == settings::Control::Check);
        CHECK(appliesNow(k));
    }
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[notify]\nwhisper = 0\n"));
    CHECK(!u.next.notify.whisper && u.waiting.empty() && u.applied.size() == 1);
}

TEST(notify_only_from_behind_and_only_what_is_on) {
    NotifySettings s;
    CHECK(notify::wanted(s, notify::Kind::Whisper, false));
    CHECK(!notify::wanted(s, notify::Kind::Whisper, true));  // in front: the player sees it
    s.whisper = false;
    CHECK(!notify::wanted(s, notify::Kind::Whisper, false));
    CHECK(notify::wanted(s, notify::Kind::Death, false));
    s.on = false;
    CHECK(!notify::wanted(s, notify::Kind::Death, false));
}

TEST(notify_a_burst_shows_once) {
    notify::Gate g;
    CHECK(g.pass(notify::Kind::Whisper, 1000));
    CHECK(!g.pass(notify::Kind::Whisper, 1000 + notify::Gate::kGapMs - 1));  // a flood of whispers: one
    CHECK(g.pass(notify::Kind::Party, 1500));                                  // another kind: its own
    CHECK(g.pass(notify::Kind::Whisper, 1000 + notify::Gate::kGapMs));
}

TEST(notify_its_words) {
    using notify::Kind;
    CHECK_EQ(notify::text(settings::Lang::En, Kind::Whisper, "Priest", "are you there?"),
             std::string("Whisper from Priest: are you there?"));
    CHECK_EQ(notify::text(settings::Lang::En, Kind::Party, "Heroes", ""),
             std::string("Invitation to the party Heroes"));
    CHECK_EQ(notify::text(settings::Lang::En, Kind::Trade, "Merchant", ""), std::string("Merchant offers a trade"));
    CHECK_EQ(notify::text(settings::Lang::En, Kind::Death, "", ""), std::string("The character died"));
    CHECK(!notify::text(settings::Lang::Uk, Kind::Guild, "Kyiv", "").empty());
    CHECK(!notify::text(settings::Lang::Ru, Kind::Friend, "Sage", "").empty());
}
