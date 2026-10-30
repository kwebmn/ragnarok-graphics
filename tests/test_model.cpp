// The settings app's model (stage 2 spec §4) over the stand-ins of tests/fake_windows.cpp.
#include "app/model.hpp"
#include "core/effect_groups.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/navigation.hpp"
#include "core/settings/reg_file.hpp"
#include "fake_windows.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

namespace {

const IniSetting& setting(const char* sectionKey) { return *findSetting(sectionKey); }

// 2.8: the settings and the backups in uaro-graphics\ (core/layout)
const std::wstring kIni = L"uaro-graphics\\uaro-graphics.ini";
const std::wstring kBackups = L"uaro-graphics\\backup";

// A backup as the model writes it (spec §4.6): files.txt, the ini, personal.reg.
void backup(const std::wstring& name, const std::string& ini) {
    fake::Windows& w = fake::windows();
    const std::wstring dir = fake::path(kBackups + L"\\" + name);
    w.folders.insert(fake::path(L"uaro-graphics"));
    w.folders.insert(fake::path(kBackups));
    w.folders.insert(dir);
    w.files[dir + L"\\files.txt"] = "uaro-graphics.ini\tuaro-graphics.ini\r\n";
    w.files[dir + L"\\uaro-graphics.ini"] = ini;
    RegCopy personal;
    fake::gameKey(personal, 1920, 1080, false);
    w.files[dir + L"\\personal.reg"] = toRegFile(personal, app::gameKeyPaths().personalRoot);
}

// 3.0.9-beta.7: the mark of the world scale's reset, written to a file read without it; 3.0.11-beta.58: and the
// autopilot's move (a file without a set is on the automatic one: keep_fps 60 with it) with its mark
const std::vector<IniEdit> kMarked{
    {"utility", "world_scale_reset", "1"}, {"frames", "keep_fps", "60"}, {"utility", "autopilot", "1"}};

// A key of an ini as readIni gives it; "<none>" when it is not there.
std::string valueOf(const std::string& ini, const std::string& sectionKey) {
    const auto v = readIni(ini);
    const auto it = v.find(sectionKey);
    return it == v.end() ? "<none>" : it->second;
}

std::wstring twoDigits(int i) { return std::wstring(1, wchar_t(L'0' + i / 10)) + wchar_t(L'0' + i % 10); }

int effectiveDword(const char* name) {
    const auto v = effectiveValue(fake::windows().registry, "", name);
    return v && v->asDword() ? int(*v->asDword()) : -1;
}

}  // namespace

TEST(model_reads_the_client_folder_and_the_game_key) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[renderer]\r\nbackend = uaro\r\n[quality]\r\nsharpen = 70\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK_EQ(m.token(setting("quality.sharpen")), std::string("70"));
    CHECK_EQ(m.pendingCount(), size_t(0));
    CHECK(m.screenNow().game == (Size{1920, 1080}));
    CHECK(m.screenNow().choice.window == (Size{3360, 1890}));
}

TEST(model_restoring_the_oldest_of_twenty_backups_keeps_it) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[quality]\r\nsharpen = 90\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    for (int i = 0; i < 20; ++i)
        backup(L"2026-10-01-1200" + twoDigits(i), "[quality]\r\nsharpen = " + std::to_string(i) + "\r\n");
    app::Model m;
    CHECK_EQ(m.now().backups.size(), size_t(20));
    const std::wstring oldest = m.now().backups.back();  // newest first
    CHECK(oldest == L"2026-10-01-120000");
    CHECK(m.restore(oldest, Lang::En).empty());
    CHECK(w.folders.count(fake::path(kBackups + L"\\" + oldest)) > 0);
    // 3.0.9-beta.7: as it was, with the world scale's mark the app writes when it reads a file without it
    CHECK_EQ(w.files[fake::path(kIni)], applyIniEdits("[quality]\r\nsharpen = 0\r\n", kMarked));
}

TEST(model_a_backup_keeps_the_twenty_newest) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[quality]\r\nsharpen = 90\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    for (int i = 0; i < 20; ++i) backup(L"2026-10-01-1200" + twoDigits(i), "[quality]\r\nsharpen = 1\r\n");
    app::Model m;
    m.setToken(setting("quality.sharpen"), "40");
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(m.now().backups.size(), size_t(20));
    CHECK(m.now().backups.front() == L"2026-10-13-120000");  // this run's
    CHECK(m.now().backups.back() == L"2026-10-01-120001");   // the oldest went
}

TEST(model_first_run_can_be_applied_with_nothing_changed) {
    fake::Windows& w = fake::reset();  // no setup.exe yet: the game's key is in neither copy (spec §4.3)
    app::Model m;
    CHECK(m.canApply());
    CHECK_EQ(m.pendingCount(), size_t(0));  // nothing of the player's: closing asks nothing
    CHECK(m.apply(Lang::En).empty());
    CHECK(gameKeyExists(w.registry));
    CHECK_EQ(effectiveDword("WIDTH"), 1920);  // a 3360 x 1890 window at 175 %
    CHECK_EQ(effectiveDword("HEIGHT"), 1080);
    CHECK(!m.canApply());
}

TEST(model_apply_keeps_what_was_written_to_the_ini_meanwhile) {
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(kIni);
    w.files[ini] = "[renderer]\r\nbackend = uaro\r\n[quality]\r\nsharpen = 50\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    w.files[ini] = "[renderer]\r\nbackend = uaro\r\napi = d3d11\r\n[quality]\r\nsharpen = 50\r\n";  // by hand meanwhile
    m.setToken(setting("quality.sharpen"), "80");
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "renderer.api"), std::string("d3d11"));
    CHECK_EQ(valueOf(w.files[ini], "quality.sharpen"), std::string("80"));
}

TEST(model_apply_keeps_what_was_written_to_roexts_ini_meanwhile) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[renderer]\r\nbackend = uaro\r\n";
    w.files[fake::path(L"dinput.dll")] = std::string("MZ") + '\0' + "ROExt" + '\0' + ".\\dinput.ini" + '\0';
    const std::wstring roext = fake::path(L"dinput.ini");
    w.files[roext] = "[ROExt]\r\nWindowLock=1\r\nWindowWidth=1920\r\nWindowHeight=1080\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    w.files[roext] += "MouseFreedom=1\r\n";  // the player, by hand, meanwhile
    ScreenChoice c = m.screenChoice();
    c.ui = 200;  // the same 3360 x 1890 window, the interface larger: the game 1680 x 945
    m.setScreen(c);
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[roext], "roext.mousefreedom"), std::string("1"));
    CHECK_EQ(valueOf(w.files[roext], "roext.windowwidth"), std::string("1680"));
    CHECK_EQ(valueOf(w.files[roext], "roext.windowheight"), std::string("945"));
}

// 3.0.1-beta.8: ROExt's dinput8.dll moved aside, [client] roext on - the driver does ROExt's work: its window from
// [roext], the Screen page's size written there; off, or a client that never had ROExt - the game's own window
TEST(model_roext_in_the_driver_takes_the_windows_size) {
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(kIni);
    const std::string roextDll = std::string("MZ") + '\0' + "ROExt" + '\0' + ".\\dinput8.ini" + '\0';
    w.files[ini] = "[renderer]\r\nbackend = uaro\r\n[roext]\r\nwindow_lock = 1\r\nwindow_width = 1920\r\n"
                   "window_height = 1080\r\n";
    w.files[fake::path(L"uaro-graphics\\original\\dinput8.dll")] = roextDll;
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK(m.now().roextInDriver && m.now().roext.active && m.now().roext.lock && m.now().roextModule < 0);
    CHECK(m.now().roext.size == (Size{1920, 1080}));
    CHECK(m.screenNow().choice.window == (Size{3360, 1890}));
    ScreenChoice c = m.screenChoice();
    c.ui = 200;  // the same 3360 x 1890 window, the interface larger: the game 1680 x 945
    m.setScreen(c);
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "roext.window_width"), std::string("1680"));
    CHECK_EQ(valueOf(w.files[ini], "roext.window_height"), std::string("945"));
    CHECK(!w.files.count(fake::path(L"dinput8.ini")));
    w.gameRunning = true;  // as with ROExt's own ini: the window's size waits for the game to close
    m.refreshRunning();
    CHECK(m.screenWaits());
    fake::Windows& v = fake::reset();
    v.files[ini] = "[client]\r\nroext = 0\r\n";
    v.files[fake::path(L"uaro-graphics\\original\\dinput8.dll")] = roextDll;
    fake::gameKey(v.registry.common, 1920, 1080, false);
    app::Model off;
    CHECK(!off.now().roextInDriver && !off.now().roext.active);
    fake::reset();  // no ROExt ever: nothing changes for it
    app::Model never;
    CHECK(!never.now().roextInDriver && !never.now().roext.active);
}

TEST(model_a_registry_value_refused_leaves_the_others_written) {
    fake::Windows& w = fake::reset();  // a first run: every value the game needs (spec §4.3)
    w.refused = {"WIDTH"};
    app::Model m;
    const std::wstring err = m.apply(Lang::En);
    CHECK(err.find(L"WIDTH (error 5)") != std::wstring::npos);  // named (spec §4.5)
    CHECK_EQ(effectiveDword("HEIGHT"), 1080);                   // and the others written all the same
    CHECK(effectiveValue(w.registry, "", "DEVICENAME").has_value());
}

TEST(model_a_backup_holds_the_registry_as_it_is_at_the_backup) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[quality]\r\nsharpen = 50\r\n";
    fake::gameKey(w.registry.personal, 1920, 1080, false);
    app::Model m;
    w.registry.personal.keys["ShortcutItem\\uaRO"]["Slot1"] = RegValue::dword(7);  // the game, at a map change
    m.setToken(setting("quality.sharpen"), "80");
    CHECK(m.apply(Lang::En).empty());
    RegCopy saved;
    CHECK(fromRegFile(w.files[fake::path(kBackups + L"\\2026-10-13-120000\\personal.reg")],
                      app::gameKeyPaths().personalRoot, saved));
    const RegValue* slot = saved.find("ShortcutItem\\uaRO", "Slot1");
    CHECK(slot && slot->asDword() && *slot->asDword() == 7);
}

TEST(model_a_screen_that_waits_for_the_game_leaves_the_rest_written) {
    // While the game runs, ROExt's ini is not written (spec §4.1): a screen that needs it waits for the game to
    // close, everything else is written now (0.6.0 review M4).
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(kIni);
    w.files[ini] = "[renderer]\r\nbackend = uaro\r\n[quality]\r\nsharpen = 50\r\n";
    w.files[fake::path(L"dinput.dll")] = std::string("MZ") + '\0' + "ROExt" + '\0' + ".\\dinput.ini" + '\0';
    const std::wstring roext = fake::path(L"dinput.ini");
    w.files[roext] = "[ROExt]\r\nWindowLock=1\r\nWindowWidth=1920\r\nWindowHeight=1080\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.gameRunning = true;
    app::Model m;
    ScreenChoice c = m.screenChoice();
    c.ui = 200;  // the same 3360 x 1890 window, the interface larger: the game 1680 x 945
    m.setScreen(c);
    m.setToken(setting("quality.sharpen"), "80");
    CHECK(!m.apply(Lang::En).empty());  // it says the screen waits
    CHECK_EQ(valueOf(w.files[ini], "quality.sharpen"), std::string("80"));
    CHECK_EQ(effectiveDword("WIDTH"), 1920);
    CHECK_EQ(valueOf(w.files[roext], "roext.windowwidth"), std::string("1920"));
    CHECK_EQ(m.pendingCount(), size_t(1));  // the screen
    w.gameRunning = false;
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(effectiveDword("WIDTH"), 1680);
    CHECK_EQ(valueOf(w.files[roext], "roext.windowwidth"), std::string("1680"));
}

TEST(model_while_the_game_runs_only_a_screen_roext_sizes_waits) {
    // Spec §4.1, 0.6.1 minor 6: ROExt's ini waits while the game runs, and only a window's plan writes it - the full
    // screen does not: the Screen page greys the window's controls only.
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[renderer]\r\nbackend = uaro\r\n";
    w.files[fake::path(L"dinput.dll")] = std::string("MZ") + '\0' + "ROExt" + '\0' + ".\\dinput.ini" + '\0';
    w.files[fake::path(L"dinput.ini")] = "[ROExt]\r\nWindowLock=1\r\nWindowWidth=1920\r\nWindowHeight=1080\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.gameRunning = true;
    app::Model m;
    CHECK(m.screenWaits());  // the game's window: ROExt sizes it
    ScreenChoice c = m.screenChoice();
    c.show = Show::Fullscreen;
    m.setScreen(c);
    CHECK(!m.screenWaits());
    c.show = Show::Window;
    m.setScreen(c);
    w.gameRunning = false;
    m.refreshRunning();
    CHECK(!m.screenWaits());
}

TEST(model_a_first_run_while_the_game_runs_writes_the_key_and_leaves_roexts_ini) {
    // No key yet: the game waits in its setup.exe loop (registry.md §2) with ROExt loaded. The key goes now - it is
    // what lets the game start - and ROExt's ini waits for the game to close (spec §4.1; 0.6.1 review I1).
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[renderer]\r\nbackend = uaro\r\n";
    w.files[fake::path(L"dinput.dll")] = std::string("MZ") + '\0' + "ROExt" + '\0' + ".\\dinput.ini" + '\0';
    const std::wstring roext = fake::path(L"dinput.ini");
    w.files[roext] = "[ROExt]\r\nWindowLock=1\r\nWindowWidth=1600\r\nWindowHeight=900\r\n";
    w.gameRunning = true;
    app::Model m;
    ScreenChoice c = m.screenChoice();
    c.ui = 200;  // the 2800 x 1575 window ROExt makes, the interface larger: the game 1400 x 788
    m.setScreen(c);
    CHECK(!m.apply(Lang::En).empty());  // it says the screen waits
    CHECK(gameKeyExists(w.registry));
    CHECK_EQ(effectiveDword("WIDTH"), 1400);
    CHECK_EQ(valueOf(w.files[roext], "roext.windowwidth"), std::string("1600"));
    w.gameRunning = false;
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[roext], "roext.windowwidth"), std::string("1400"));
}

TEST(model_game_settings_are_written_while_the_game_is_closed) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[renderer]\r\nbackend = uaro\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    m.setRegWord(*findRegSetting("SPRITEMODE"), "1");
    m.setRegWord(*findRegSetting("Option\\isEffectOn"), "0");
    m.setRegWord(*findRegSetting("ShortCutList\\0"), "/lv");
    CHECK_EQ(m.pendingCount(), size_t(3));
    CHECK(m.regWord(*findRegSetting("SPRITEMODE")) == "1");
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(effectiveDword("SPRITEMODE"), 1);
    const auto effect = effectiveValue(w.registry, "Option", "isEffectOn");
    CHECK(effect && effect->asDword() && *effect->asDword() == 0u);
    const auto macro = effectiveValue(w.registry, "ShortCutList", "0");
    CHECK(macro && macro->text == "/lv");
    CHECK_EQ(m.pendingCount(), size_t(0));
}

TEST(model_game_settings_the_game_writes_wait_while_it_runs) {
    // Spec §4.1: ISLIGHTMAP the game never writes - it goes now; SPRITEMODE it writes at exit - it waits.
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[renderer]\r\nbackend = uaro\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    m.setRegWord(*findRegSetting("ISLIGHTMAP"), "0");
    m.setRegWord(*findRegSetting("SPRITEMODE"), "0");
    w.gameRunning = true;
    m.refreshRunning();
    CHECK(!m.regOpen(*findRegSetting("SPRITEMODE")));
    CHECK(m.regOpen(*findRegSetting("ISLIGHTMAP")));
    CHECK(!m.apply(Lang::En).empty());  // it says what waits
    CHECK_EQ(effectiveDword("ISLIGHTMAP"), 0);
    CHECK_EQ(effectiveDword("SPRITEMODE"), -1);  // not there yet
    CHECK_EQ(m.pendingCount(), size_t(1));
    w.gameRunning = false;
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(effectiveDword("SPRITEMODE"), 0);
}

TEST(model_a_game_setting_equal_to_setup_s_stays_the_player_s) {
    // 2.4.2: a value equal to the common copy's (setup.exe's) stays in the personal one - until now it was deleted, and
    // setup.exe was in charge of it again
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.common.keys[""]["SOUNDMODE"] = RegValue::dword(1);
    w.registry.personal.exists = true;
    w.registry.personal.keys[""]["SOUNDMODE"] = RegValue::dword(2);
    app::Model m;
    m.setRegWord(*findRegSetting("SOUNDMODE"), "1");
    CHECK(m.apply(Lang::En).empty());
    const RegValue* mine = w.registry.personal.find("", "SOUNDMODE");
    CHECK(mine && mine->asDword() == 1u);
    CHECK_EQ(effectiveDword("SOUNDMODE"), 1);
}

TEST(model_apply_takes_what_only_setup_exe_s_copy_holds) {
    // 2.4.2: Apply writes every value the game reads from setup.exe's copy alone into the personal one - setup.exe
    // changing it later changes nothing the game sees
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.common.keys[""]["SOUNDMODE"] = RegValue::dword(1);
    w.registry.personal.exists = true;
    w.registry.personal.keys["Option"]["isEffectOn"] = RegValue::dword(0);
    app::Model m;
    m.setRegWord(*findRegSetting("TEXTUREMODE"), "1");
    CHECK(m.apply(Lang::En).empty());
    for (const char* name : {"WIDTH", "HEIGHT", "ISFULLSCREENMODE", "BITPERPIXEL", "SOUNDMODE"})
        CHECK(w.registry.personal.find("", name) != nullptr);
    w.registry.common.keys[""]["SOUNDMODE"] = RegValue::dword(2);  // setup.exe run afterwards
    CHECK_EQ(effectiveDword("SOUNDMODE"), 1);
}

TEST(model_start_writes_the_game_key_when_there_is_none) {
    // 2.4.2: without the game's key in either copy the game starts setup.exe and waits for it - the app writes what the
    // game needs as soon as it starts, not only at Apply
    fake::Windows& w = fake::reset();
    app::Model m;
    CHECK(m.ownGameKey(Lang::En).empty());
    CHECK(gameKeyExists(w.registry));
    CHECK_EQ(effectiveDword("WIDTH"), 1920);  // a 3360 x 1890 window at 175 %
    CHECK_EQ(effectiveDword("SOUNDMODE"), 1);
    CHECK_EQ(m.pendingCount(), size_t(0));
}

TEST(model_start_takes_what_only_setup_exe_s_copy_holds) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.gameRunning = true;  // it reads its key at its start: taken over once it is closed
    app::Model running;
    CHECK(running.ownGameKey(Lang::En).empty());
    CHECK(w.registry.personal.find("", "WIDTH") == nullptr);
    w.gameRunning = false;
    app::Model m;
    CHECK(m.ownGameKey(Lang::En).empty());
    CHECK(w.registry.personal.find("", "WIDTH") != nullptr);
    CHECK(w.registry.personal.find("", "BITPERPIXEL") != nullptr);
}

TEST(model_presets_and_revert_take_the_game_s_quality_too) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    m.setRegWord(*findRegSetting("TEXTUREMODE"), "1");
    m.applyPreset(Preset::Speed);
    CHECK(m.regWord(*findRegSetting("TEXTUREMODE")) == "2");  // 0.87.1: full size, as the others
    m.applyPreset(Preset::Maximum);
    CHECK(m.regWord(*findRegSetting("TEXTUREMODE")) == "2");
    CHECK(m.regWord(*findRegSetting("SPRITEMODE")) == "2");
    m.setRegWord(*findRegSetting("FOG"), "0");
    CHECK(m.canRevert());
    CHECK(m.revert(Lang::En).empty());
    CHECK(m.regWord(*findRegSetting("FOG")) == "1");
    CHECK_EQ(m.pendingCount(), size_t(0));
}

TEST(model_the_preset_shown_counts_the_game_s_quality) {
    // Spec §8: from 0.6.2 a preset sets the game's quality too; with it changed by hand the Service page names none.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    m.applyPreset(Preset::Speed);
    CHECK(m.preset() == Preset::Speed);
    m.setRegWord(*findRegSetting("TEXTUREMODE"), "1");
    CHECK(!m.preset());
    m.applyPreset(Preset::Maximum);
    CHECK(m.preset() == Preset::Maximum);
}

TEST(model_skins_are_the_folders_of_skin_and_the_intro_a_file) {
    fake::Windows& w = fake::reset();
    w.folders.insert(fake::path(L"skin"));
    w.folders.insert(fake::path(L"skin\\default"));
    w.folders.insert(fake::path(L"skin\\Blue"));
    app::Model m;
    CHECK(m.skins() == (std::vector<std::wstring>{L"Blue", L"default"}));
    CHECK(!m.hasIntro());
    w.files[fake::path(L"openning.bik")] = "BIK";
    CHECK(m.hasIntro());
}

TEST(model_a_closed_game_s_own_writes_are_read_again) {
    // 0.6.2 review I1: the game writes Option, UIRectInfo, its quality and the login as it closes; the pages show them
    // and Apply plans against them once it is closed - the app was open all the while.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.gameRunning = true;
    app::Model m;
    w.registry.personal.exists = true;
    w.registry.personal.keys["Option"]["isEffectOn"] = RegValue::dword(0);  // the game, at exit
    w.gameRunning = false;
    CHECK(m.refreshRunning());
    CHECK(m.regWord(*findRegSetting("Option\\isEffectOn")) == "0");
    m.setRegWord(*findRegSetting("Option\\isEffectOn"), "1");  // effects back on: a change, not a no-op
    CHECK_EQ(m.pendingCount(), size_t(1));
}

TEST(model_forget_login_clears_the_saved_one) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.personal.exists = true;
    w.registry.personal.keys[""]["ID"] = RegValue::sz("player");
    w.gameRunning = true;  // the game writes it from its login window: refused while it runs (spec §4.1)
    app::Model m;
    CHECK(!m.forgetLogin(Lang::En).empty());
    CHECK(w.registry.personal.find("", "ID")->text == "player");
    w.gameRunning = false;
    CHECK(m.forgetLogin(Lang::En).empty());
    CHECK(w.registry.personal.find("", "ID")->text == "");
}

TEST(model_game_settings_go_to_a_file_and_back) {
    // Spec §8: the file holds the whole key as the game reads it - both copies, the personal one winning - under the
    // personal copy's path; loading replaces the personal copy, a copy of the present one made first.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.common.keys[""]["SOUNDMODE"] = RegValue::dword(1);
    w.registry.personal.exists = true;
    w.registry.personal.keys["Option"]["isEffectOn"] = RegValue::dword(0);
    w.registry.personal.keys["ShortcutItem\\uaRO"]["SITEM000"] = RegValue::sz("Red Potion");
    app::Model m;
    const std::wstring file = fake::path(L"mine.reg");
    CHECK(m.saveGameSettings(file, Lang::En).empty());
    RegCopy saved;
    CHECK(fromRegFile(w.files[file], app::gameKeyPaths().personalRoot, saved));
    CHECK(saved.find("", "SOUNDMODE") && saved.find("Option", "isEffectOn") &&
          saved.find("ShortcutItem\\uaRO", "SITEM000"));
    w.registry.personal = RegCopy{};  // another PC: nothing of the player's there yet
    app::Model other;
    CHECK(other.loadGameSettings(file, Lang::En).empty());
    const RegValue* item = w.registry.personal.find("ShortcutItem\\uaRO", "SITEM000");
    CHECK(item && item->text == "Red Potion");
    CHECK(w.registry.personal.find("Option", "isEffectOn") != nullptr);
    CHECK(!other.now().backups.empty());
}

TEST(model_the_settings_file_leaves_a_legacy_password_out) {
    // 0.6.2 review M3: the client never writes PASSWORD and drops it when it reads it (registry.md); an old one
    // must not travel in a file a player may pass on.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.common.keys[""]["PASSWORD"] = RegValue::sz("secret");
    app::Model m;
    const std::wstring file = fake::path(L"mine.reg");
    CHECK(m.saveGameSettings(file, Lang::En).empty());
    RegCopy saved;
    CHECK(fromRegFile(w.files[file], app::gameKeyPaths().personalRoot, saved));
    CHECK(saved.find("", "WIDTH") != nullptr);
    CHECK(saved.find("", "PASSWORD") == nullptr);
}

TEST(model_game_settings_load_and_reset_wait_for_the_game_to_close) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.files[fake::path(L"mine.reg")] = toRegFile(effectiveCopy(w.registry), app::gameKeyPaths().personalRoot);
    w.gameRunning = true;  // the game writes most of it at exit (spec §4.1)
    app::Model m;
    CHECK(!m.loadGameSettings(fake::path(L"mine.reg"), Lang::En).empty());
    CHECK(!m.resetWindowLayout(Lang::En).empty());
    CHECK(!w.registry.personal.exists);
}

TEST(model_a_file_without_the_game_key_is_refused) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"other.reg")] =
        "Windows Registry Editor Version 5.00\r\n\r\n[HKEY_CURRENT_USER\\Software\\Other]\r\n\"A\"=dword:00000001\r\n";
    app::Model m;
    CHECK(!m.loadGameSettings(fake::path(L"other.reg"), Lang::En).empty());
    CHECK(!m.loadGameSettings(fake::path(L"none.reg"), Lang::En).empty());
    CHECK(!w.registry.personal.exists);
}

TEST(model_a_file_with_a_part_of_the_key_is_refused) {
    // 0.6.2 review I2: regedit's export of one folder of the key (the chat macros, say) is not the whole of it;
    // loading it in place of the personal copy would take the hotbars and the windows away.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.personal.exists = true;
    w.registry.personal.keys["ShortcutItem\\uaRO"]["SITEM000"] = RegValue::sz("Red Potion");
    w.files[fake::path(L"macros.reg")] = "Windows Registry Editor Version 5.00\r\n\r\n[" +
                                         app::gameKeyPaths().personalRoot + "\\ShortCutList]\r\n\"0\"=\"/lv\"\r\n";
    app::Model m;
    CHECK(!m.loadGameSettings(fake::path(L"macros.reg"), Lang::En).empty());
    CHECK(w.registry.personal.find("ShortcutItem\\uaRO", "SITEM000") != nullptr);
}

TEST(model_a_window_layout_reset_writes_the_personal_copy) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.personal.exists = true;
    w.registry.personal.keys["UIRectInfo"]["ITEMWNDINFO.X"] = RegValue::dword(1700);
    app::Model m;
    CHECK(m.resetWindowLayout(Lang::En).empty());
    const auto x = effectiveValue(w.registry, "UIRectInfo", "ITEMWNDINFO.X");
    CHECK(x && x->asDword() && *x->asDword() == 0xFFFFFFFFu);
    CHECK(!m.now().backups.empty());
}

TEST(model_profiles_keep_and_bring_back_every_setting) {
    // 0.55: a profile is uaro-graphics.ini with the player's changes in, under a name; loading one writes it back.
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(kIni);
    w.files[ini] = "[renderer]\r\nbackend = uaro\r\n[quality]\r\nsharpen = 70\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK(m.profiles().empty());
    m.setToken(setting("quality.sharpen"), "40");  // not applied yet: the profile takes it
    CHECK(m.saveProfile(L"Слабкий ноутбук", Lang::En).empty());
    CHECK(!m.saveProfile(L"bad/name", Lang::En).empty());
    CHECK(!m.saveProfile(L"", Lang::En).empty());
    CHECK(m.profiles() == (std::vector<std::wstring>{L"Слабкий ноутбук"}));
    CHECK_EQ(valueOf(w.files[ini], "quality.sharpen"), std::string("70"));  // the ini as it was
    CHECK(m.loadProfile(L"Слабкий ноутбук", Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "quality.sharpen"), std::string("40"));
    CHECK_EQ(m.pendingCount(), size_t(0));
    CHECK(!m.loadProfile(L"gone", Lang::En).empty());
    CHECK(m.deleteProfile(L"Слабкий ноутбук", Lang::En).empty());
    CHECK(m.profiles().empty());
    CHECK(app::Model::profileNameOk(L"Max 4K_v2-b"));
    CHECK(!app::Model::profileNameOk(L" leading"));
    CHECK(!app::Model::profileNameOk(std::wstring(41, L'a')));
}

TEST(model_writes_several_keys_now_and_undo_brings_them_back) {
    // 2.2: a set made the player's own in a running game - every switch in one write, Undo takes them all back
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(kIni);
    w.files[ini] = "[effects]\r\npreset = strong\r\ngrass = 0\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    const std::vector<std::pair<const IniSetting*, std::string>> changes = {
        {&setting("effects.grass"), "1"}, {&setting("effects.preset"), "custom"}};
    CHECK(m.writeNow(changes, Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "effects.grass"), std::string("1"));
    CHECK_EQ(valueOf(w.files[ini], "effects.preset"), std::string("custom"));
    CHECK(m.revert(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "effects.grass"), std::string("0"));
    CHECK_EQ(valueOf(w.files[ini], "effects.preset"), std::string("strong"));
}

TEST(model_profiles_and_reset_keep_the_statistics_answer) {
    // 2.2.2: the player's answer about the statistics and this computer's number stay through a profile loaded or
    // a reset (as the language does): a profile saved before the question would ask it again and count a new player
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(kIni);
    w.files[ini] = "[quality]\r\nsharpen = 70\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK(m.saveProfile(L"old", Lang::En).empty());  // made before the question
    CHECK(m.setStats(true, Lang::En).empty());
    const std::string id = valueOf(w.files[ini], "stats.id");
    CHECK(id.size() == 16);
    CHECK(m.loadProfile(L"old", Lang::En).empty());
    CHECK(m.statsAsked());
    CHECK_EQ(valueOf(w.files[ini], "stats.send"), std::string("1"));
    CHECK_EQ(valueOf(w.files[ini], "stats.id"), id);
    CHECK(m.resetDefaults(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "stats.send"), std::string("1"));
    CHECK_EQ(valueOf(w.files[ini], "stats.id"), id);
}

TEST(model_asks_about_the_automatic_reports_once) {
    // 3.0.0-beta.1 (decision 5): a report by itself when a game ends badly - asked once, at the first start; the
    // answer and the asking stay through a profile loaded or a reset, as the statistics' answer does
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(kIni);
    w.files[ini] = "[report]\r\nauto = 0\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK(!m.reportAsked());
    CHECK(m.saveProfile(L"old", Lang::En).empty());  // made before the question
    CHECK(m.setReportAuto(true, Lang::En).empty());
    CHECK(m.reportAsked() && m.now().config.reportAuto);
    CHECK_EQ(valueOf(w.files[ini], "report.auto"), std::string("1"));
    CHECK(m.loadProfile(L"old", Lang::En).empty());
    CHECK(m.reportAsked() && m.now().config.reportAuto);
    CHECK(m.resetDefaults(Lang::En).empty());
    CHECK(m.reportAsked() && m.now().config.reportAuto);
    CHECK(m.setReportAuto(false, Lang::En).empty());  // «Ні»: asked all the same
    CHECK(m.reportAsked() && !m.now().config.reportAuto);
    CHECK_EQ(valueOf(w.files[ini], "report.auto"), std::string("0"));
}

// 3.0.0-beta.2: the update channel - with no line the app's start writes the build's («Бета» for a beta build), so a
// tester updated to a release build stays on «Бета»; the player's choice is written at once.
TEST(model_records_the_channel_at_its_start) {
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(kIni);
    const std::wstring version = fake::path(L"uaro-graphics\\version.txt");
    w.files[ini] = "[display]\r\nmode = window\r\n\r\n[update]\r\n; the template's words\r\n";
    w.files[version] = "3.0.0-beta.2 build 2026-10-23 03:00\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK(m.channel() == update::Channel::Beta);
    m.recordChannel(Lang::En);
    CHECK_EQ(valueOf(w.files[ini], "update.channel"), std::string("beta"));
    CHECK(w.files[ini].find("[update]") == w.files[ini].rfind("[update]"));  // into the template's section
    w.files[version] = "3.0.0 build 2026-10-24 03:00\r\n";  // updated to a release
    app::Model later;
    later.recordChannel(Lang::En);
    CHECK(later.channel() == update::Channel::Beta);
    CHECK(later.setChannel(update::Channel::Release, Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "update.channel"), std::string("release"));
    CHECK(later.channel() == update::Channel::Release);
}

// 3.0.0-beta.2: a switched channel forgets the other branch's offer the last check kept - the game's start never asks
// to update to a build of the channel the player left (that build would put the install back on «Бета»)
TEST(model_switching_the_channel_forgets_the_other_branchs_offer) {
    fake::Windows& w = fake::reset();
    const std::wstring kept = fake::path(L"uaro-graphics\\update.ini");
    w.files[fake::path(kIni)] = "[update]\r\nchannel = beta\r\n";
    w.files[fake::path(L"uaro-graphics\\version.txt")] = "3.0.0-beta.2 build 2026-10-23 03:00\r\n";
    w.files[kept] = "[release]\r\nformat = 2\r\nversion = 3.0.0-beta.3\r\n[package]\r\nref = " + std::string(40, 'a') +
                    "\r\nfile = release/uaro-graphics.zip\r\nsize = 10\r\nsha256 = " + std::string(64, 'b') +
                    "\r\n[local]\r\nskip = 3.0.0-beta.1\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    std::string version;
    CHECK(update::askAtStart(w.files[kept], "3.0.0-beta.2", version));  // the beta's offer, kept
    CHECK(m.setChannel(update::Channel::Beta, Lang::En).empty());       // the same channel: kept
    CHECK(update::askAtStart(w.files[kept], "3.0.0-beta.2", version));
    CHECK(m.setChannel(update::Channel::Release, Lang::En).empty());
    CHECK(!update::askAtStart(w.files[kept], "3.0.0-beta.2", version));
    CHECK(w.files[kept].find("skip = 3.0.0-beta.1") != std::string::npos);  // a version rolled back from stays skipped
}

TEST(model_keeps_the_channel_through_a_reset) {
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(kIni);
    w.files[ini] = "[quality]\r\nsharpen = 70\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK(m.saveProfile(L"old", Lang::En).empty());  // made before the choice
    CHECK(m.setChannel(update::Channel::Beta, Lang::En).empty());
    CHECK(m.loadProfile(L"old", Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "update.channel"), std::string("beta"));
    CHECK(m.resetDefaults(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "update.channel"), std::string("beta"));
}

// 2.6: the modes, the window's place and the version whose notes were shown - the app's own keys of [utility].
TEST(model_new_install_starts_simple) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK(!m.usedBefore());
    CHECK(m.startMode() == Mode::Simple);
    CHECK(!m.savedPlace());
    CHECK(m.seenVersion().empty());
}

TEST(model_used_install_starts_detailed) {
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(kIni);
    w.files[ini] = "[utility]\r\nlanguage = ru\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK(m.usedBefore());
    CHECK(m.startMode() == Mode::Detailed);
    m.saveMode(Mode::Developer);
    CHECK(m.startMode() == Mode::Developer);
    CHECK_EQ(valueOf(w.files[ini], "utility.mode"), std::string("developer"));
    CHECK_EQ(m.pendingCount(), size_t(0));  // the app's own key: no change for Apply
}

TEST(model_mode_place_and_version_survive_reset) {
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(kIni);
    w.files[ini] = "[quality]\r\nsharpen = 70\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK(m.saveProfile(L"old", Lang::En).empty());  // made before the keys
    m.saveMode(Mode::Developer);
    m.savePlace({10, 20, 1110, 740, true});
    m.saveSeenVersion("2.6.0");
    CHECK(m.resetDefaults(Lang::En).empty());
    CHECK(m.loadProfile(L"old", Lang::En).empty());
    CHECK(m.startMode() == Mode::Developer);
    const auto place = m.savedPlace();
    CHECK(place && place->left == 10 && place->bottom == 740 && place->maximized);
    CHECK_EQ(m.seenVersion(), std::string("2.6.0"));
}

TEST(model_recommends_a_word_each_row_takes) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    for (const IniSetting& s : iniSettings())
        CHECK(s.control == Control::Line || validToken(s, m.recommended(s)));
    CHECK_EQ(m.recommended(setting("renderer.gpu")), std::string("auto"));
}

TEST(model_settles_an_old_files_groups_at_its_start) {
    // 2.6.22: the ten groups before written into the effects once, when the app opens - the same picture, the old
    // lines gone, the file kept in a backup first
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[effects]\r\npreset = custom\r\ngrass = 1\r\ntorch = 1\r\n"
                                                "group_nature = 0\r\ngroup_light = 1\r\ngroup_light_strength = 100\r\n";
    app::Model m;
    const std::string ini = w.files[fake::path(kIni)];
    CHECK_EQ(valueOf(ini, "effects.grass"), std::string("0"));
    CHECK_EQ(valueOf(ini, "effects.torch"), std::string("1"));
    CHECK_EQ(valueOf(ini, "effects.group_nature"), std::string("<none>"));
    CHECK_EQ(valueOf(ini, "effects.group_light"), std::string("<none>"));
    CHECK_EQ(valueOf(ini, "effects.group_light_strength"), std::string("<none>"));
    CHECK(!effect_groups::hasLegacy(m.now().config.effects));
    CHECK(!m.now().config.effects.grass);
    CHECK(w.folders.count(fake::path(kBackups)) == 1);
    // a file of the new groups is left as it is (3.0.9-beta.7: with the world scale's mark - a file of now)
    const std::string fresh = "[effects]\r\npreset = auto\r\ngroup_plants = 0\r\n[utility]\r\nworld_scale_reset = 1\r\n"
                              "autopilot = 1\r\n";  // 3.0.11-beta.58: and the autopilot's mark
    w.files[fake::path(kIni)] = fresh;
    m.load();
    CHECK(w.files[fake::path(kIni)] == fresh);
}

TEST(model_takes_the_old_world_scale_back_at_its_start) {
    // 3.0.9-beta.7 (the owner, R-3FS6YX): the old cards' 75 % of the automatic setup back to 100 once, when the app
    // opens; the share the player picks after it stays
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[quality]\r\nworld_scale = 75\r\n";
    app::Model m;
    const std::string ini = w.files[fake::path(kIni)];
    CHECK_EQ(valueOf(ini, "quality.world_scale"), std::string("100"));
    CHECK_EQ(valueOf(ini, "utility.world_scale_reset"), std::string("1"));
    CHECK_EQ(m.token(setting("quality.world_scale")), std::string("100"));
    CHECK_EQ(m.now().config.worldScale, 100);
    w.files[fake::path(kIni)] = "[quality]\r\nworld_scale = 75\r\n[utility]\r\nworld_scale_reset = 1\r\n";
    m.load();
    CHECK_EQ(valueOf(w.files[fake::path(kIni)], "quality.world_scale"), std::string("75"));
    // no file: nothing written
    fake::Windows& empty = fake::reset();
    app::Model none;
    CHECK(empty.files.count(fake::path(kIni)) == 0);
}

TEST(model_moves_a_2x_install_into_uaro_graphics) {
    fake::Windows& w = fake::reset();
    w.folders.insert(fake::path(L"uaro-graphics"));
    w.files[fake::path(L"uaro-graphics.ini")] = "[quality]\r\nsharpen = 70\r\n";  // as 2.7.58 left it
    w.files[fake::path(kIni)] = "; the 2.x package's template\r\n";
    w.files[fake::path(L"uaro-textures.zip")] = "pack";
    app::Model m;
    CHECK(m.paths().ini == fake::path(kIni));
    CHECK_EQ(m.token(setting("quality.sharpen")), std::string("70"));
    CHECK(w.files.count(fake::path(L"uaro-graphics.ini")) == 0);
    CHECK(m.paths().pack == fake::path(L"uaro-graphics\\textures\\uaro-textures.zip"));
    CHECK(w.files.count(m.paths().pack) > 0);
}

TEST(model_a_pack_that_could_not_move_is_used_where_it_is) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-textures.zip")] = "pack";
    w.locked.insert(fake::path(L"uaro-textures.zip"));  // uaro-render64 makes the pack meanwhile
    app::Model m;
    CHECK(m.paths().pack == fake::path(L"uaro-textures.zip"));
}

TEST(model_restoring_a_2x_backup_writes_the_ini_in_uaro_graphics) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[quality]\r\nsharpen = 90\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    backup(L"2026-10-01-120000", "[quality]\r\nsharpen = 5\r\n");  // its files.txt names 2.x's root ini
    app::Model m;
    CHECK(m.restore(L"2026-10-01-120000", Lang::En).empty());
    CHECK_EQ(w.files[fake::path(kIni)], applyIniEdits("[quality]\r\nsharpen = 5\r\n", kMarked));
    CHECK(w.files.count(fake::path(L"uaro-graphics.ini")) == 0);
}

TEST(model_a_new_backup_names_the_ini_where_it_is_now) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(kIni)] = "[quality]\r\nsharpen = 90\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    m.setToken(setting("quality.sharpen"), "40");
    CHECK(m.apply(Lang::En).empty());
    const std::string list = w.files[fake::path(kBackups + L"\\2026-10-13-120000\\files.txt")];
    CHECK(list.find("uaro-graphics.ini\tuaro-graphics\\uaro-graphics.ini\r\n") != std::string::npos);
}

TEST(model_removes_an_old_pad_dll_and_puts_back_what_it_moved) {
    // 2.9: the pad is inside the driver - a 2.x uaRO Pad dinput.dll would be a second one
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"dinput.dll")] = std::string("MZ") + '\0' + "uaro-pad.ini" + '\0' + "uaro-pad.log";
    w.files[fake::path(L"dinput.dll.before-pad")] = "ROExt";
    w.files[fake::path(L"uaro-pad.moved")] = "dinput.dll.before-pad";
    app::Model m;
    CHECK_EQ(w.files[fake::path(L"dinput.dll")], std::string("ROExt"));
    CHECK(w.files.count(fake::path(L"dinput.dll.before-pad")) == 0);
    CHECK(w.files.count(fake::path(L"uaro-pad.moved")) == 0);
}

TEST(model_leaves_an_old_pad_dll_while_the_game_runs) {
    fake::Windows& w = fake::reset();
    const std::string pad = std::string("MZ") + '\0' + "uaro-pad.log";
    w.files[fake::path(L"dinput.dll")] = pad;
    w.gameRunning = true;  // it is the game's: it cannot go now
    app::Model m;
    CHECK_EQ(w.files[fake::path(L"dinput.dll")], pad);
}

namespace {

void aCard(fake::Windows& w, const char* name, unsigned vendor, unsigned long long vram) {
    w.system.windows.major = 10;
    w.system.d3d12Probed = true;
    w.system.d3d12.ok = true;
    w.system.d3d12.adapter = name;
    w.system.d3d12.vendor = vendor;
    w.system.d3d12.vramMb = vram;
    w.system.d3d11.ok = true;
    w.system.d3d11.level = 0xc100;
}

}  // namespace

// 2.12: the first start sets a fresh install up for its card, and records the card and the screen.
TEST(model_sets_a_fresh_install_up) {
    fake::Windows& w = fake::reset();
    aCard(w, "NVIDIA GeForce RTX 4070", 0x10de, 12282);
    app::Model m;
    CHECK(m.startSetup(Lang::Uk) == FirstStart::Apply);
    const std::string ini = w.files[fake::path(kIni)];
    CHECK_EQ(valueOf(ini, "renderer.api"), std::string("auto"));
    CHECK(valueOf(ini, "utility.machine").rfind("NVIDIA GeForce RTX 4070|12282|", 0) == 0);
    CHECK(m.lastAutoSetup() && m.lastAutoSetup()->tier == Tier::High);
}

TEST(first_start_leaves_a_used_install_alone) {
    fake::Windows& w = fake::reset();
    aCard(w, "NVIDIA GeForce RTX 4070", 0x10de, 12282);
    w.folders.insert(fake::path(L"uaro-graphics"));
    w.files[fake::path(kIni)] = "[utility]\r\nlanguage = uk\r\n[quality]\r\nsharpen = 13\r\n";
    app::Model m;
    CHECK(m.startSetup(Lang::Uk) == FirstStart::Record);
    const std::string ini = w.files[fake::path(kIni)];
    CHECK_EQ(valueOf(ini, "quality.sharpen"), std::string("13"));  // the player's own
    CHECK(!valueOf(ini, "utility.machine").empty());
    CHECK(!m.lastAutoSetup());
    w.system.d3d12.adapter = "AMD Radeon RX 7800 XT";  // another card in the computer
    app::Model again;
    CHECK(again.startSetup(Lang::Uk) == FirstStart::Ask);
    CHECK_EQ(valueOf(w.files[fake::path(kIni)], "quality.sharpen"), std::string("13"));
}

// 2.12: «Безпечний режим» and «Вимкнути uaRO Graphics» in one press; «Звичайний режим» forgets an automatic one too.
TEST(model_safe_mode_and_switching_off) {
    fake::Windows& w = fake::reset();
    app::Model m;
    CHECK(m.setSafeMode(true, Lang::Uk).empty());
    CHECK_EQ(valueOf(w.files[fake::path(kIni)], "renderer.safe_mode"), std::string("1"));
    w.folders.insert(fake::path(L"uaro-graphics\\cache"));
    w.files[fake::path(L"uaro-graphics\\cache\\safe-mode.txt")] = "2 starts in a row ended before their first picture";
    w.files[fake::path(L"uaro-graphics\\cache\\failed-starts.txt")] = "2";
    CHECK(m.safeModeNoted());
    CHECK(m.setSafeMode(false, Lang::Uk).empty());
    CHECK_EQ(valueOf(w.files[fake::path(kIni)], "renderer.safe_mode"), std::string("0"));
    CHECK(!m.safeModeNoted());
    CHECK(w.files.count(fake::path(L"uaro-graphics\\cache\\failed-starts.txt")) == 0);
    CHECK(m.setSwitchedOff(true, Lang::Uk).empty());
    CHECK_EQ(valueOf(w.files[fake::path(kIni)], "renderer.api"), std::string("system"));
    CHECK(m.setSwitchedOff(false, Lang::Uk).empty());
    CHECK_EQ(valueOf(w.files[fake::path(kIni)], "renderer.api"), std::string("auto"));
}

TEST(rows_follow_the_card_the_game_draws_on) {
    // 2.15: a laptop's NVIDIA card and its Intel one - the DLSS rows follow the card the Render page chose
    fake::Windows& w = fake::reset();
    w.system.adapters.push_back(win::AdapterProbe{"NVIDIA GeForce RTX 4060 Laptop GPU", 0x10de, 8188, true, 0xc100});
    w.system.adapters.push_back(win::AdapterProbe{"Intel(R) UHD Graphics", 0x8086, 128, true, 0xc100, 8192});
    app::Model m;
    CHECK(fitsMachine("quality.dlss", m.machine()));  // auto: the fast one
    m.setToken(setting("renderer.gpu"), "Intel(R) UHD Graphics");
    CHECK(!fitsMachine("quality.dlss", m.machine()));
}

// 3.0.11-beta.58 (the autopilot): an install set up before goes over to the automatic set when the app opens (the
// driver too, at the game's start) - its card's memory's set and keep_fps 45 to auto and 60; the player's own set stays
TEST(model_moves_an_untouched_install_to_the_autopilot_at_its_start) {
    fake::Windows& w = fake::reset();
    aCard(w, "AMD Radeon RX 550", 0x1002, 2048);  // weak by its memory: the setup gave it «Низькі»
    w.folders.insert(fake::path(L"uaro-graphics"));
    w.files[fake::path(kIni)] =
        "[effects]\r\npreset = weak\r\n[frames]\r\nkeep_fps = 45\r\n[utility]\r\nlanguage = uk\r\n";
    app::Model m;
    const std::string ini = w.files[fake::path(kIni)];
    CHECK_EQ(valueOf(ini, "effects.preset"), std::string("auto"));
    CHECK_EQ(valueOf(ini, "frames.keep_fps"), std::string("60"));
    CHECK_EQ(valueOf(ini, "utility.autopilot"), std::string("1"));
    w.files[fake::path(kIni)] = "[effects]\r\npreset = strong\r\n[utility]\r\nlanguage = uk\r\n";  // picked by hand
    app::Model own;
    CHECK_EQ(valueOf(w.files[fake::path(kIni)], "effects.preset"), std::string("strong"));
    CHECK_EQ(valueOf(w.files[fake::path(kIni)], "utility.autopilot"), std::string("1"));
}

// 3.0.11-beta.58 (the autopilot): a fresh install, a new card or a new screen has its card judged again - the driver's
// verdict for the card before goes; the same machine keeps it
TEST(model_forgets_the_card_verdict_for_a_new_machine) {
    fake::Windows& w = fake::reset();
    aCard(w, "NVIDIA GeForce RTX 4070", 0x10de, 12282);
    w.folders.insert(fake::path(L"uaro-graphics"));
    const std::wstring record = fake::path(L"uaro-graphics\\card-level.txt");
    w.files[record] = "v2 1 1 NVIDIA GeForce GTX 750\n";
    app::Model m;
    CHECK(m.startSetup(Lang::Uk) == FirstStart::Apply);
    CHECK(w.files.count(record) == 0);
    w.files[record] = "v2 3 1 NVIDIA GeForce RTX 4070\n";
    app::Model same;
    CHECK(same.startSetup(Lang::Uk) == FirstStart::Nothing);
    CHECK(w.files.count(record) == 1);
    w.system.d3d12.adapter = "AMD Radeon RX 7800 XT";
    app::Model other;
    CHECK(other.startSetup(Lang::Uk) == FirstStart::Ask);
    CHECK(w.files.count(record) == 0);
}
