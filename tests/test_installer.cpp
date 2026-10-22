// 2.10: the installer (app/installer) over the stand-ins of tests/fake_windows.cpp - no .bat does this any more.
#include <map>
#include <string>

#include "app/files.hpp"
#include "app/installer.hpp"
#include "fake_windows.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

const std::wstring kStaged = L"uaro-graphics\\update\\files";

// A file of the client folder, its folders made.
void put(const std::wstring& relative, const std::string& bytes) {
    fake::Windows& w = fake::windows();
    for (size_t at = relative.find(L'\\'); at != std::wstring::npos; at = relative.find(L'\\', at + 1))
        w.folders.insert(fake::path(relative.substr(0, at)));
    w.files[fake::path(relative)] = bytes;
}

std::string at(const std::wstring& relative) {
    const auto it = fake::windows().files.find(fake::path(relative));
    return it == fake::windows().files.end() ? "<none>" : it->second;
}

// A uaRO client: the game and its data.
fake::Windows& client() {
    fake::Windows& w = fake::reset();
    put(L"uaRO.exe", "MZ uaRO");
    put(L"data.grf", "GRF");
    return w;
}

// A package unpacked as runUpdate leaves it: version `v`; `same` keeps the exe and the renderer of 1.0.0.
std::wstring stage(const std::string& v, bool same = false) {
    put(kStaged + L"\\uaro-graphics\\ddraw.dll", "uaRO Graphics ddraw " + v);
    put(kStaged + L"\\uaro-graphics\\uaro-graphics.exe", "uaRO Graphics exe " + (same ? std::string("1.0.0") : v));
    put(kStaged + L"\\uaro-graphics\\uaro-render64.exe", "render64 " + (same ? std::string("1.0.0") : v));
    put(kStaged + L"\\uaro-graphics\\version.txt", v + " build 2026-10-22 18:00\r\n");
    put(kStaged + L"\\install.bat", "rem uaRO Graphics: the older update.bat's way in");
    return fake::path(kStaged);
}

// The client folder but the installer's own work folder.
std::map<std::wstring, std::string> outsideWork() {
    std::map<std::wstring, std::string> out;
    const std::wstring work = fake::path(L"uaro-graphics\\update");
    for (const auto& f : fake::windows().files)
        if (f.first.compare(0, work.size(), work) != 0) out.insert(f);
    return out;
}

}  // namespace

TEST(installer_puts_a_package_into_a_fresh_folder) {
    client();
    const app::installer::Result r = app::installer::putPackage(app::exeFolder(), stage("1.0.0"), false);
    CHECK(r.ok);
    CHECK_EQ(at(L"ddraw.dll"), std::string("uaRO Graphics ddraw 1.0.0"));
    CHECK_EQ(at(L"uaro-graphics.exe"), std::string("uaRO Graphics exe 1.0.0"));
    CHECK_EQ(at(L"uaro-graphics\\version.txt"), std::string("1.0.0 build 2026-10-22 18:00\r\n"));
    CHECK_EQ(at(L"install.bat"), std::string("<none>"));  // no .bat goes into the folder
    CHECK(!app::installer::canRollBack(app::exeFolder()));
}

TEST(installer_keeps_the_replaced_files_in_previous) {
    client();
    CHECK(app::installer::putPackage(app::exeFolder(), stage("1.0.0"), false).ok);
    CHECK(app::installer::putPackage(app::exeFolder(), stage("2.0.0", true), false).ok);
    CHECK_EQ(at(L"ddraw.dll"), std::string("uaRO Graphics ddraw 2.0.0"));
    CHECK_EQ(at(L"uaro-graphics\\previous\\ddraw.dll"), std::string("uaRO Graphics ddraw 1.0.0"));
    CHECK_EQ(at(L"uaro-graphics\\previous\\uaro-graphics\\ddraw.dll"), std::string("uaRO Graphics ddraw 1.0.0"));
    CHECK_EQ(at(L"uaro-graphics\\previous\\uaro-graphics\\version.txt"),
             std::string("1.0.0 build 2026-10-22 18:00\r\n"));
    CHECK_EQ(at(L"uaro-graphics\\previous\\uaro-graphics.exe"), std::string("<none>"));  // the same in both
    CHECK_EQ(at(L"uaro-graphics\\previous\\uaro-graphics\\uaro-render64.exe"), std::string("<none>"));
    CHECK_EQ(app::installer::previousVersion(app::exeFolder()), std::string("1.0.0"));
}

TEST(installer_undoes_everything_when_a_file_cannot_be_put) {
    fake::Windows& w = client();
    CHECK(app::installer::putPackage(app::exeFolder(), stage("1.0.0"), false).ok);
    CHECK(app::installer::putPackage(app::exeFolder(), stage("2.0.0"), false).ok);
    const std::map<std::wstring, std::string> before = outsideWork();
    w.locked.insert(fake::path(L"uaro-graphics\\uaro-render64.exe"));  // the texture pack being made holds it
    const app::installer::Result r = app::installer::putPackage(app::exeFolder(), stage("3.0.0"), false);
    CHECK(!r.ok);
    CHECK(!r.error.empty());
    CHECK(outsideWork() == before);  // the files of 2.0.0 and the previous 1.0.0, as they were
}

TEST(installer_moves_a_foreign_ddraw_to_dgvoodoo) {
    client();
    put(L"ddraw.dll", "dgVoodoo DDraw");
    CHECK(app::installer::putPackage(app::exeFolder(), stage("1.0.0"), false).ok);
    CHECK_EQ(at(L"uaro-graphics\\dgvoodoo\\DDraw.dll"), std::string("dgVoodoo DDraw"));
    CHECK_EQ(at(L"ddraw.dll"), std::string("uaRO Graphics ddraw 1.0.0"));
    CHECK(!app::installer::canRollBack(app::exeFolder()));  // dgVoodoo's is not a version of ours
}

TEST(installer_rolls_back_to_previous) {
    client();
    CHECK(app::installer::putPackage(app::exeFolder(), stage("1.0.0"), false).ok);
    CHECK(app::installer::putPackage(app::exeFolder(), stage("2.0.0"), false).ok);
    CHECK(app::installer::canRollBack(app::exeFolder()));
    const app::installer::Result r = app::installer::rollBack(app::exeFolder());
    CHECK(r.ok);
    CHECK_EQ(at(L"ddraw.dll"), std::string("uaRO Graphics ddraw 1.0.0"));
    CHECK_EQ(at(L"uaro-graphics.exe"), std::string("uaRO Graphics exe 1.0.0"));
    CHECK_EQ(at(L"uaro-graphics\\version.txt"), std::string("1.0.0 build 2026-10-22 18:00\r\n"));
    CHECK(!app::installer::canRollBack(app::exeFolder()));
}

TEST(installer_keeps_previous_when_nothing_changed_or_on_a_repair) {
    client();
    CHECK(app::installer::putPackage(app::exeFolder(), stage("1.0.0"), false).ok);
    CHECK(app::installer::putPackage(app::exeFolder(), stage("2.0.0"), false).ok);
    CHECK(app::installer::putPackage(app::exeFolder(), stage("2.0.0"), false).ok);  // the same again
    CHECK_EQ(app::installer::previousVersion(app::exeFolder()), std::string("1.0.0"));
    put(L"ddraw.dll", "uaRO Graphics ddraw, damaged");
    CHECK(app::installer::putPackage(app::exeFolder(), stage("2.0.0"), true).ok);  // a repair
    CHECK_EQ(at(L"ddraw.dll"), std::string("uaRO Graphics ddraw 2.0.0"));
    CHECK_EQ(at(L"uaro-graphics\\previous\\ddraw.dll"), std::string("uaRO Graphics ddraw 1.0.0"));
}

// What install.bat did after the files, now finishInstall's.
TEST(finish_install_puts_the_template_for_a_fresh_install) {
    client();
    put(L"uaro-graphics\\defaults\\uaro-graphics.ini", "; the template\r\n[display]\r\nmode = window\r\n");
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    CHECK_EQ(at(L"uaro-graphics\\uaro-graphics.ini"), std::string("; the template\r\n[display]\r\nmode = window\r\n"));
    CHECK(fake::windows().folders.count(fake::path(L"uaro-graphics\\logs")) == 1);
}

TEST(finish_install_upgrades_an_old_settings_file) {
    client();
    put(L"uaro-graphics\\defaults\\uaro-graphics.ini", "; the template\r\n");
    put(L"uaro-graphics.ini", "[display]\r\nmode = window\r\n");  // a 0.3 file in the root
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    CHECK_EQ(at(L"uaro-graphics.ini"), std::string("<none>"));
    const std::string ini = at(L"uaro-graphics\\uaro-graphics.ini");
    CHECK(ini.rfind("[display]\r\nmode = window\r\n", 0) == 0);
    CHECK(ini.find("\r\nsharpen = 50\r\n") != std::string::npos);
}

TEST(finish_install_moves_dgvoodoos_files_aside_and_an_old_pad_away) {
    client();
    put(L"D3DImm.dll", "dgVoodoo D3DImm");
    put(L"dgVoodoo.conf", "[General]");
    put(L"dinput.dll", std::string("MZ") + '\0' + "uaro-pad.log");
    put(L"dinput8.dll", "ROExt");
    put(L"uaro-pad.moved", "dinput8.dll\r\n");
    put(L"uaro-graphics\\pad\\dinput.dll", "pad");
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    CHECK_EQ(at(L"uaro-graphics\\dgvoodoo\\D3DImm.dll"), std::string("dgVoodoo D3DImm"));
    CHECK_EQ(at(L"D3DImm.dll"), std::string("<none>"));
    CHECK_EQ(at(L"uaro-graphics\\dgvoodoo\\dgVoodoo.conf"), std::string("[General]"));
    CHECK_EQ(at(L"dgVoodoo.conf"), std::string("[General]"));  // install.bat copied it: the player's stays
    CHECK_EQ(at(L"dinput.dll"), std::string("ROExt"));
    CHECK_EQ(at(L"uaro-pad.moved"), std::string("<none>"));
    CHECK(fake::windows().folders.count(fake::path(L"uaro-graphics\\pad")) == 0);
}

TEST(finish_install_removes_our_old_scripts_only) {
    client();
    put(L"install.bat", "@echo off\r\nrem uaRO Graphics: puts our ddraw.dll into this uaRO client folder.\r\n");
    put(L"update.bat", "@echo off\r\nrem uaRO Graphics: downloads the newest build\r\n");
    put(L"get-dlss.bat", "@echo off\r\nrem uaRO Graphics: NVIDIA DLSS\r\n");
    put(L"uninstall.bat", "@echo off\r\nrem another program's\r\n");
    put(L"run.bat", "start uaRO.exe\r\n");
    put(L"uaro-graphics-update\\files\\uaro-graphics\\ddraw.dll", "left by an old update.bat");
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    CHECK_EQ(at(L"install.bat"), std::string("<none>"));
    CHECK_EQ(at(L"update.bat"), std::string("<none>"));
    CHECK_EQ(at(L"get-dlss.bat"), std::string("<none>"));
    CHECK_EQ(at(L"uninstall.bat"), std::string("@echo off\r\nrem another program's\r\n"));
    CHECK_EQ(at(L"run.bat"), std::string("start uaRO.exe\r\n"));
    CHECK(fake::windows().folders.count(fake::path(L"uaro-graphics-update")) == 0);
}

TEST(finish_install_leaves_the_scripts_while_one_runs) {
    client();
    put(L"update.bat", "@echo off\r\nrem uaRO Graphics: downloads the newest build\r\n");  // it runs this
    put(L"uaro-graphics-update\\files\\install.bat", "rem uaRO Graphics");
    CHECK(app::installer::finishInstall(app::exeFolder(), false).ok);
    CHECK(at(L"update.bat") != "<none>");
    CHECK(fake::windows().folders.count(fake::path(L"uaro-graphics-update")) == 1);
}

TEST(finish_install_writes_the_apps_entry) {
    client();
    put(L"uaro-graphics\\version.txt", "2.10.0 build 2026-10-22 18:00\r\n");
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    std::map<std::wstring, std::wstring>& e = fake::windows().appsEntry;
    CHECK(e[L"DisplayName"] == L"uaRO Graphics");
    CHECK(e[L"DisplayVersion"] == L"2.10.0");
    CHECK(e[L"UninstallString"] == L"\"" + app::exeFolder() + L"\\uaro-graphics.exe\" --uninstall");
    CHECK(e[L"InstallLocation"] == app::exeFolder());
}
