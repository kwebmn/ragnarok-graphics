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

// 3.0.7-beta.6 (the user chose 1, 5, 6, 7, 8, 9, 10 «и чтобы это в конфигураторе настраивалось»): an attack, a buff
// ending, the party's deaths and low HP, the party's and the guild's chat, the name said near, the server gone, a
// sound - each a box on the «Гра» page, the two thresholds sliders
TEST(notify_the_new_kinds_in_the_configurator) {
    const NotifySettings d = parseConfig("").notify;
    CHECK(d.attack && d.buffEnding && d.partyDeath && d.partyLowHp && d.partyChat && d.guildChat && d.mention &&
          d.disconnect && d.sound);
    CHECK(d.buffSeconds == 10 && d.partyHp == 30);
    const NotifySettings s = parseConfig("[notify]\nattack = 0\nbuff_ending = 0\nparty_death = 0\nparty_low_hp = 0\n"
                                         "party_chat = 0\nguild_chat = 0\nmention = 0\ndisconnect = 0\nsound = 0\n"
                                         "buff_seconds = 25\nparty_hp = 45\n")
                                 .notify;
    CHECK(!s.attack && !s.buffEnding && !s.partyDeath && !s.partyLowHp && !s.partyChat && !s.guildChat &&
          !s.mention && !s.disconnect && !s.sound);
    CHECK(s.buffSeconds == 25 && s.partyHp == 45);
    const NotifySettings far = parseConfig("[notify]\nbuff_seconds = 999\nparty_hp = 1\n").notify;  // kept in range
    CHECK(far.buffSeconds == 60 && far.partyHp == 10);
    for (const char* k : {"notify.attack", "notify.buff_ending", "notify.party_death", "notify.party_low_hp",
                          "notify.party_chat", "notify.guild_chat", "notify.mention", "notify.disconnect",
                          "notify.sound", "notify.buff_seconds", "notify.party_hp"}) {
        const settings::IniSetting* row = settings::findSetting(k);
        CHECK(row != nullptr);
        if (!row) continue;
        CHECK(row->page == settings::Page::Game);
        CHECK(appliesNow(k));
        CHECK(settings::tokenOf(*row, parseConfig("")) == settings::tokenOf(*row, parseConfig("")));
    }
    const settings::IniSetting* seconds = settings::findSetting("notify.buff_seconds");
    CHECK(seconds && seconds->control == settings::Control::Slider && settings::tokenOf(*seconds, parseConfig("")) ==
                                                                          std::string("10"));
    const settings::IniSetting* party = settings::findSetting("notify.party_hp");
    CHECK(party && party->control == settings::Control::Slider && settings::tokenOf(*party, parseConfig("")) ==
                                                                        std::string("30"));
    const settings::IniSetting* attack = settings::findSetting("notify.attack");
    CHECK(attack && attack->control == settings::Control::Check &&
          settings::tokenOf(*attack, parseConfig("[notify]\nattack = 0\n")) == std::string("0"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[notify]\nattack = 0\nparty_hp = 50\n"));
    CHECK(!u.next.notify.attack && u.next.notify.partyHp == 50 && u.waiting.empty() && u.applied.size() == 2);
}

TEST(notify_the_party_s_and_the_guild_s_from_the_main_window_alone) {
    // several game windows (a helper in the same party and guild): what all of them hear told by the main one alone
    // - the one in front, or last in front; what is this character's own by any of them
    NotifySettings s;
    using notify::Kind;
    for (Kind k : {Kind::PartyChat, Kind::GuildChat, Kind::PartyDeath, Kind::PartyLowHp}) {
        CHECK(notify::wanted(s, k, false, true));
        CHECK(!notify::wanted(s, k, false, false));
    }
    for (Kind k : {Kind::Attack, Kind::BuffEnding, Kind::Mention, Kind::Disconnect, Kind::Whisper, Kind::Death})
        CHECK(notify::wanted(s, k, false, false));
    CHECK(!notify::wanted(s, Kind::Attack, true, true));  // in front: the player sees it
    s.attack = false;
    CHECK(!notify::wanted(s, Kind::Attack, false, true));
}

TEST(notify_the_new_words) {
    using notify::Kind;
    const auto en = [](Kind k, const std::string& who, const std::string& what) {
        return notify::text(settings::Lang::En, k, who, what);
    };
    CHECK_EQ(en(Kind::Attack, "", "57"), std::string("The character is under attack: -57 HP"));
    CHECK_EQ(en(Kind::Attack, "", ""), std::string("The character is under attack"));
    CHECK_EQ(en(Kind::BuffEnding, "Blessing", "10"), std::string("Blessing ends in 10 s"));
    CHECK_EQ(en(Kind::PartyDeath, "Priest", ""), std::string("Priest of the party died"));
    CHECK_EQ(en(Kind::PartyLowHp, "Priest", "25"), std::string("Priest of the party: 25% HP"));
    CHECK_EQ(en(Kind::PartyChat, "Priest", "heal?"), std::string("Party - Priest: heal?"));
    CHECK_EQ(en(Kind::GuildChat, "Sage", "endow?"), std::string("Guild - Sage: endow?"));
    CHECK_EQ(en(Kind::Mention, "Knight", "sora, here"), std::string("Your name in the chat - Knight: sora, here"));
    CHECK_EQ(en(Kind::Disconnect, "", ""), std::string("The connection to the server is lost"));
    for (settings::Lang l : {settings::Lang::Uk, settings::Lang::Ru})
        for (Kind k : {Kind::Attack, Kind::BuffEnding, Kind::PartyDeath, Kind::PartyLowHp, Kind::PartyChat,
                       Kind::GuildChat, Kind::Mention, Kind::Disconnect})
            CHECK(!notify::text(l, k, "A", "1").empty());
}

TEST(notify_an_attack_once_then_after_a_calm) {
    notify::Attacks a;
    CHECK(a.hit(1000));                                   // the first hit: told
    CHECK(!a.hit(2000));                                  // the fight goes on: the flashing says it
    CHECK(!a.hit(2000 + notify::Attacks::kCalmMs - 1));   // a hit within the calm keeps it going
    CHECK(a.hit(2000 + notify::Attacks::kCalmMs - 1 + notify::Attacks::kCalmMs));  // a calm, then attacked again
}

TEST(notify_who_said_what_in_the_chat) {
    std::string who, what;
    notify::speaker("Priest : heal? : now", who, what);
    CHECK(who == "Priest" && what == "heal? : now");
    notify::speaker("no name here", who, what);
    CHECK(who.empty() && what == "no name here");
}

TEST(notify_the_character_s_name_said) {
    // UTF-8, letters' case aside, a whole word (not inside another's name)
    CHECK(notify::mentions("hey sora, come here", "Sora"));
    CHECK(notify::mentions("SORA!", "Sora"));
    CHECK(!notify::mentions("Soraya is here", "Sora"));
    CHECK(!notify::mentions("hey Kasora", "Sora"));
    CHECK(notify::mentions("Dark Knight, help", "Dark Knight"));
    CHECK(notify::mentions("\xd0\xbf\xd1\x80\xd0\xb8\xd0\xb2\xd1\x96\xd1\x82, \xd0\x9e\xd0\x9a\xd0\xa1\xd0\x90\xd0\x9d"
                           "\xd0\x90!",
                           "\xd0\x9e\xd0\xba\xd1\x81\xd0\xb0\xd0\xbd\xd0\xb0"));  // «привіт, ОКСАНА!» - «Оксана»
    CHECK(!notify::mentions("\xd0\x9e\xd0\xba\xd1\x81\xd0\xb0\xd0\xbd\xd0\xb0\xd0\xb4\xd0\xb7\xd0\xb5",
                            "\xd0\x9e\xd0\xba\xd1\x81\xd0\xb0\xd0\xbd\xd0\xb0"));  // «Оксанадзе»: another name
    CHECK(!notify::mentions("anything", ""));
}

// 3.0.7-beta.7 (the user: «надо сделать чтобы когда хп и сп восстановились»): HP and SP both back - told once, again
// after one of them fell well under the mark
TEST(notify_hp_and_sp_back) {
    notify::Recovery r;
    CHECK(!r.status(5, 4000, 100));  // the most not known yet
    CHECK(!r.status(6, 4000, 100));
    CHECK(!r.status(8, 1000, 100));
    CHECK(!r.status(7, 1000, 100));  // full as the game starts: nothing to tell
    CHECK(!r.status(5, 3000, 100));  // 75 %: down
    CHECK(!r.status(7, 500, 100));
    CHECK(!r.status(5, 4000, 100));  // HP back, SP not yet
    CHECK(r.status(7, 1000, 100));   // both: told
    CHECK(!r.status(7, 990, 100));   // a little under: not again
    CHECK(!r.status(7, 1000, 100));
    CHECK(!r.status(5, 3500, 100));  // 87 %: well under (kRearm)
    CHECK(r.status(5, 4000, 100));   // back: told again
    CHECK(!r.status(5, 1000, 80));   // a mark of 80 %
    CHECK(r.status(5, 3200, 80));
    CHECK(!r.status(24, 5, 100));    // the weight and the rest: not HP or SP
}

TEST(notify_hp_and_sp_back_in_the_configurator) {
    const NotifySettings d = parseConfig("").notify;
    CHECK(d.recovered && d.recoveredPercent == 100);
    const NotifySettings s = parseConfig("[notify]\nrecovered = 0\nrecovered_percent = 85\n").notify;
    CHECK(!s.recovered && s.recoveredPercent == 85);
    CHECK(parseConfig("[notify]\nrecovered_percent = 20\n").notify.recoveredPercent == 50);  // kept in range
    const settings::IniSetting* box = settings::findSetting("notify.recovered");
    CHECK(box && box->page == settings::Page::Game && box->control == settings::Control::Check);
    const settings::IniSetting* mark = settings::findSetting("notify.recovered_percent");
    CHECK(mark && mark->control == settings::Control::Slider && settings::tokenOf(*mark, parseConfig("")) ==
                                                                      std::string("100"));
    CHECK(appliesNow("notify.recovered") && appliesNow("notify.recovered_percent"));
    CHECK(notify::wanted(NotifySettings{}, notify::Kind::Recovered, false, false));  // the character's own
    CHECK_EQ(notify::text(settings::Lang::En, notify::Kind::Recovered, "", "100"),
             std::string("HP and SP back to 100%"));
    CHECK(!notify::text(settings::Lang::Uk, notify::Kind::Recovered, "", "100").empty());
}

// 3.0.9-beta.4: the developer answered a report - told from behind as the rest ([notify] report_answer); in front the
// game's own card tells of it
TEST(notify_tells_of_a_reports_answer) {
    const NotifySettings d = parseConfig("").notify;
    CHECK(d.reportAnswer);
    CHECK(notify::wanted(d, notify::Kind::Answer, false, false));  // any game window: the player's own report
    CHECK(!notify::wanted(d, notify::Kind::Answer, true));
    CHECK(!notify::wanted(parseConfig("[notify]\nreport_answer = 0\n").notify, notify::Kind::Answer, false));
    CHECK_EQ(notify::text(settings::Lang::Uk, notify::Kind::Answer, "R-7BDUE3", "Потрібна інформація"),
             std::string("Відповідь на звернення R-7BDUE3: Потрібна інформація"));
    CHECK(settings::findSetting("notify.report_answer") != nullptr);  // in the settings app with the other kinds
}
