#include <string>
#include <vector>

#include "core/audio_output.hpp"
#include "core/config.hpp"
#include "core/live_config.hpp"
#include "core/settings/navigation.hpp"
#include "core/settings/schema.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.4-beta.1 (the user: the game played on the monitor's HDMI output with Bluetooth headphones on; «выбирать на
// каком устройстве выводить звук»): [sound] output - Windows' endpoint id, "" (auto) Windows' default.
namespace {
const std::string kPhones = "{0.0.0.00000000}.{6a1b2c3d-0000-4000-8000-000000000001}";
const std::string kMonitor = "{0.0.0.00000000}.{6a1b2c3d-0000-4000-8000-000000000002}";
}  // namespace

TEST(audio_output_default_when_none_chosen) {
    const audio_output::Target t = audio_output::pick("", {kPhones, kMonitor});
    CHECK(t.id.empty() && !t.away);
}

TEST(audio_output_the_chosen_one_while_it_is_there) {
    const audio_output::Target t = audio_output::pick(kPhones, {kMonitor, kPhones});
    CHECK_EQ(t.id, kPhones);
    CHECK(!t.away);
    // Windows writes the same id in capitals in places: the same device
    std::string upper = kPhones;
    for (char& c : upper) c = char(c >= 'a' && c <= 'z' ? c - 32 : c);
    CHECK_EQ(audio_output::pick(upper, {kMonitor, kPhones}).id, upper);
}

TEST(audio_output_the_default_while_the_chosen_one_is_away) {
    const audio_output::Target t = audio_output::pick(kPhones, {kMonitor});
    CHECK(t.id.empty() && t.away);
    CHECK(audio_output::pick(kPhones, {}).away);
}

TEST(audio_output_reopened_when_the_target_changes_or_its_device_is_lost) {
    CHECK(!audio_output::reopen("", audio_output::pick("", {kMonitor, kPhones}), false));
    CHECK(audio_output::reopen("", audio_output::pick("", {kMonitor, kPhones}), true));  // lost: opened again
    CHECK(audio_output::reopen("", audio_output::pick(kPhones, {kMonitor, kPhones}), false));  // chosen
    CHECK(!audio_output::reopen(kPhones, audio_output::pick(kPhones, {kMonitor, kPhones}), false));
    CHECK(audio_output::reopen(kPhones, audio_output::pick("", {kMonitor, kPhones}), false));  // back to Windows'
    // the chosen headphones off: Windows' default; back: on them again
    CHECK(audio_output::reopen(kPhones, audio_output::pick(kPhones, {kMonitor}), true));
    CHECK(!audio_output::reopen("", audio_output::pick(kPhones, {kMonitor}), false));
    CHECK(audio_output::reopen("", audio_output::pick(kPhones, {kMonitor, kPhones}), false));
}

TEST(audio_output_setting_read) {
    CHECK(parseConfig("").sound.output.empty());
    CHECK(parseConfig("[sound]\noutput = auto\n").sound.output.empty());
    const std::string mixed = "{0.0.0.00000000}.{6A1B2C3D-0000-4000-8000-000000000001}";
    CHECK_EQ(parseConfig("[sound]\noutput = " + mixed + "\n").sound.output, mixed);  // as written
    const settings::IniSetting* s = settings::findSetting("sound.output");
    CHECK(s != nullptr);
    if (!s) return;
    CHECK(s->control == settings::Control::Line && s->page == settings::Page::Sound);
    CHECK_EQ(settings::tokenOf(*s, parseConfig("")), std::string("auto"));
    CHECK_EQ(settings::tokenOf(*s, parseConfig("[sound]\noutput = " + kPhones + "\n")), kPhones);
    CHECK(settings::validToken(*s, kPhones) && settings::validToken(*s, "auto"));
    CHECK(settings::mainKey("sound.output"));  // on the Main page: the Simple mode has no Sound page
    CHECK(settings::levelOf(*s) == settings::Mode::Simple);
}

TEST(audio_output_changed_in_a_running_game) {
    CHECK(appliesNow("sound.output"));
    const LiveUpdate u = liveUpdate(parseConfig(""), parseConfig("[sound]\noutput = " + kPhones + "\n"));
    CHECK(u.waiting.empty() && u.applied.size() == 1);
    if (u.applied.size() == 1) CHECK_EQ(u.applied[0], "output auto -> " + kPhones);
    CHECK_EQ(u.next.sound.output, kPhones);
}
