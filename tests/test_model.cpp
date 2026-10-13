// The settings app's model (stage 2 spec §4) over the stand-ins of tests/fake_windows.cpp.
#include "app/model.hpp"
#include "core/settings/game_settings.hpp"
#include "core/settings/ini_edit.hpp"
#include "core/settings/reg_file.hpp"
#include "fake_windows.hpp"
#include "test.hpp"

using namespace uagfx;
using namespace uagfx::settings;

namespace {

const IniSetting& setting(const char* sectionKey) { return *findSetting(sectionKey); }

const std::wstring kBackups = L"uaro-graphics-backup";

// A backup as the model writes it (spec §4.6): files.txt, the ini, personal.reg.
void backup(const std::wstring& name, const std::string& ini) {
    fake::Windows& w = fake::windows();
    const std::wstring dir = fake::path(kBackups + L"\\" + name);
    w.folders.insert(fake::path(kBackups));
    w.folders.insert(dir);
    w.files[dir + L"\\files.txt"] = "uaro-graphics.ini\tuaro-graphics.ini\r\n";
    w.files[dir + L"\\uaro-graphics.ini"] = ini;
    RegCopy personal;
    fake::gameKey(personal, 1920, 1080, false);
    w.files[dir + L"\\personal.reg"] = toRegFile(personal, app::gameKeyPaths().personalRoot);
}

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
    w.files[fake::path(L"uaro-graphics.ini")] = "[renderer]\r\nbackend = uaro\r\n[quality]\r\nsharpen = 70\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    CHECK_EQ(m.token(setting("quality.sharpen")), std::string("70"));
    CHECK(m.backend() == Backend::Uaro);
    CHECK_EQ(m.pendingCount(), size_t(0));
    CHECK(m.screenNow().game == (Size{1920, 1080}));
    CHECK(m.screenNow().choice.window == (Size{3360, 1890}));
}

TEST(model_restoring_the_oldest_of_twenty_backups_keeps_it) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-graphics.ini")] = "[quality]\r\nsharpen = 90\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    for (int i = 0; i < 20; ++i)
        backup(L"2026-10-01-1200" + twoDigits(i), "[quality]\r\nsharpen = " + std::to_string(i) + "\r\n");
    app::Model m;
    CHECK_EQ(m.now().backups.size(), size_t(20));
    const std::wstring oldest = m.now().backups.back();  // newest first
    CHECK(oldest == L"2026-10-01-120000");
    CHECK(m.restore(oldest, Lang::En).empty());
    CHECK(w.folders.count(fake::path(kBackups + L"\\" + oldest)) > 0);
    CHECK_EQ(w.files[fake::path(L"uaro-graphics.ini")], std::string("[quality]\r\nsharpen = 0\r\n"));
}

TEST(model_a_backup_keeps_the_twenty_newest) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-graphics.ini")] = "[quality]\r\nsharpen = 90\r\n";
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
    const std::wstring ini = fake::path(L"uaro-graphics.ini");
    w.files[ini] = "[renderer]\r\nbackend = uaro\r\n[quality]\r\nsharpen = 50\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    w.files[ini] = "[renderer]\r\nbackend = dgvoodoo\r\n[quality]\r\nsharpen = 50\r\n";  // use-dgvoodoo.bat meanwhile
    m.setToken(setting("quality.sharpen"), "80");
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "renderer.backend"), std::string("dgvoodoo"));
    CHECK_EQ(valueOf(w.files[ini], "quality.sharpen"), std::string("80"));
}

TEST(model_apply_keeps_what_was_written_to_roexts_ini_meanwhile) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-graphics.ini")] = "[renderer]\r\nbackend = uaro\r\n";
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

TEST(model_reset_keeps_the_renderer_chosen_meanwhile) {
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(L"uaro-graphics.ini");
    w.files[ini] = "[renderer]\r\nbackend = uaro\r\n[quality]\r\nsharpen = 90\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    w.files[ini] = "[renderer]\r\nbackend = dgvoodoo\r\n[quality]\r\nsharpen = 90\r\n";  // use-dgvoodoo.bat meanwhile
    CHECK(m.resetDefaults(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "renderer.backend"), std::string("dgvoodoo"));
}

TEST(model_a_switch_to_dgvoodoo_writes_the_screen_the_page_shows) {
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-graphics.ini")] =
        "[renderer]\r\nbackend = uaro\r\n[display]\r\nmode = window\r\nwindow_width = 1920\r\nwindow_height = 1080\r\n";
    fake::gameKey(w.registry.common, 1680, 945, false);  // a 3360 x 1890 window, the interface at 200 %
    app::Model m;
    CHECK_EQ(m.screenNow().choice.ui, 200);
    m.setToken(setting("renderer.backend"), "dgvoodoo");
    CHECK(m.screenPlan().game == (Size{1920, 1080}));  // dgVoodoo's interface follows Windows' 175 %
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(effectiveDword("WIDTH"), 1920);
    CHECK_EQ(effectiveDword("HEIGHT"), 1080);
}

TEST(model_a_switch_to_uaro_writes_the_screen_the_page_shows) {
    fake::Windows& w = fake::reset();
    const std::wstring ini = fake::path(L"uaro-graphics.ini");
    // dgVoodoo now; [display] is left from an earlier window of ours with the interface at 200 %.
    w.files[ini] =
        "[renderer]\r\nbackend = dgvoodoo\r\n[display]\r\nmode = window\r\nwindow_width = 1680\r\nwindow_height = 945\r\n";
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    m.setToken(setting("renderer.backend"), "uaro");
    const ScreenPlan shown = m.screenPlan();
    CHECK(shown.game == (Size{1920, 1080}));
    CHECK(shown.mode == DisplayMode::Auto);  // the game's own 3360 x 1890 window, as under dgVoodoo
    CHECK(m.apply(Lang::En).empty());
    CHECK_EQ(valueOf(w.files[ini], "display.mode"), std::string("auto"));
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
    w.files[fake::path(L"uaro-graphics.ini")] = "[quality]\r\nsharpen = 50\r\n";
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
    const std::wstring ini = fake::path(L"uaro-graphics.ini");
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

TEST(model_a_first_run_while_the_game_runs_writes_the_key_and_leaves_roexts_ini) {
    // No key yet: the game waits in its setup.exe loop (registry.md §2) with ROExt loaded. The key goes now - it is
    // what lets the game start - and ROExt's ini waits for the game to close (spec §4.1; 0.6.1 review I1).
    fake::Windows& w = fake::reset();
    w.files[fake::path(L"uaro-graphics.ini")] = "[renderer]\r\nbackend = uaro\r\n";
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
    w.files[fake::path(L"uaro-graphics.ini")] = "[renderer]\r\nbackend = uaro\r\n";
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
    w.files[fake::path(L"uaro-graphics.ini")] = "[renderer]\r\nbackend = uaro\r\n";
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

TEST(model_a_game_setting_as_setup_s_leaves_setup_in_charge) {
    // Spec §4.2: a value equal to the common copy's deletes the personal one.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.common.keys[""]["SOUNDMODE"] = RegValue::dword(1);
    w.registry.personal.exists = true;
    w.registry.personal.keys[""]["SOUNDMODE"] = RegValue::dword(2);
    app::Model m;
    CHECK(m.regWord(*findRegSetting("SOUNDMODE")) == "2");
    m.setRegWord(*findRegSetting("SOUNDMODE"), "1");
    CHECK(m.apply(Lang::En).empty());
    CHECK(w.registry.personal.find("", "SOUNDMODE") == nullptr);
    CHECK_EQ(effectiveDword("SOUNDMODE"), 1);
}

TEST(model_sound_differs_from_setup_exe_s_until_taken_as_in_it) {
    // Spec §4.2: where the personal copy's sound differs from setup.exe's (the common copy), the page says so; "as in
    // setup.exe" takes setup.exe's words, and Apply deletes the personal values.
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    w.registry.common.keys[""]["SOUNDMODE"] = RegValue::dword(1);
    w.registry.common.keys[""]["NUMSAMPLETYPE"] = RegValue::dword(0);
    w.registry.personal.exists = true;
    w.registry.personal.keys[""]["SOUNDMODE"] = RegValue::dword(2);
    w.registry.personal.keys[""]["NUMSAMPLETYPE"] = RegValue::dword(0);       // the same as setup.exe's
    w.registry.personal.keys[""]["SPEAKERTYPE"] = RegValue::dword(1);         // setup.exe has none to compare
    w.registry.personal.keys["Option"]["soundVolume"] = RegValue::dword(50);  // not setup.exe's
    app::Model m;
    const auto differs = m.regSetupDiffers();
    CHECK_EQ(differs.size(), size_t(1));
    CHECK(!differs.empty() && regKey(*differs[0].first) == "SOUNDMODE" && differs[0].second == "1");
    m.regAsSetup();
    CHECK(m.regWord(*findRegSetting("SOUNDMODE")) == "1");
    CHECK(m.regSetupDiffers().empty());  // the player has taken it
    CHECK(m.apply(Lang::En).empty());
    CHECK(w.registry.personal.find("", "SOUNDMODE") == nullptr);
    CHECK(w.registry.personal.find("", "SPEAKERTYPE") != nullptr);
}

TEST(model_presets_and_revert_take_the_game_s_quality_too) {
    fake::Windows& w = fake::reset();
    fake::gameKey(w.registry.common, 1920, 1080, false);
    app::Model m;
    m.applyPreset(Preset::Speed);
    CHECK(m.regWord(*findRegSetting("TEXTUREMODE")) == "1");
    m.applyPreset(Preset::Maximum);
    CHECK(m.regWord(*findRegSetting("TEXTUREMODE")) == "2");
    CHECK(m.regWord(*findRegSetting("SPRITEMODE")) == "2");
    m.setRegWord(*findRegSetting("FOG"), "0");
    CHECK(m.canRevert());
    CHECK(m.revert(Lang::En).empty());
    CHECK(m.regWord(*findRegSetting("FOG")) == "1");
    CHECK_EQ(m.pendingCount(), size_t(0));
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
