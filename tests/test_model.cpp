// The settings app's model (stage 2 spec §4) over the stand-ins of tests/fake_windows.cpp.
#include "app/model.hpp"
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
