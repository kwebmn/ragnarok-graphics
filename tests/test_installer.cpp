// 2.10: the installer (app/installer) over the stand-ins of tests/fake_windows.cpp - no .bat does this any more.
#include <cwctype>
#include <map>
#include <set>
#include <string>

#include "app/files.hpp"
#include "app/installer.hpp"
#include "app/registry.hpp"
#include "app/text.hpp"
#include "fake_windows.hpp"
#include "test.hpp"

using namespace uagfx;
using app::utf8;

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

// Paths as Windows compares them: in any case.
std::wstring lower(std::wstring s) {
    for (wchar_t& c : s) c = wchar_t(std::towlower(c));
    return s;
}

std::map<std::wstring, std::string> filesAnyCase() {
    std::map<std::wstring, std::string> out;
    for (const auto& f : fake::windows().files) out[lower(f.first)] = f.second;
    return out;
}

std::set<std::wstring> foldersAnyCase() {
    std::set<std::wstring> out;
    for (const std::wstring& d : fake::windows().folders) out.insert(lower(d));
    return out;
}

// A client with dgVoodoo, uaRO Graphics 1.0.0 installed on it and played once.
void installedAndPlayed() {
    put(kStaged + L"\\uaro-graphics\\defaults\\uaro-graphics.ini", "; the template\r\n");
    CHECK(app::installer::putPackage(app::exeFolder(), stage("1.0.0"), false).ok);
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    put(L"uaro-graphics\\logs\\uaro-graphics.log", "a game's log");
    put(L"uaro-graphics\\cache\\pipelines.txt", "pipelines");
    put(L"uaro-graphics\\update.ini", "[release]\r\nformat = 1\r\n");
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

// 3.0.0-beta.2: a beta build put in puts the install on «Бета» (a tester joins so, by its setup too); a release build
// never changes the channel - a tester updated to a release stays on «Бета».
TEST(installing_a_beta_build_joins_the_beta_channel) {
    client();
    put(L"uaro-graphics\\uaro-graphics.ini", "[display]\r\nmode = window\r\n[update]\r\nchannel = release\r\n");
    put(L"uaro-graphics\\version.txt", "3.0.0-beta.2 build 2026-10-23 03:00\r\n");
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    CHECK(at(L"uaro-graphics\\uaro-graphics.ini").find("channel = beta") != std::string::npos);
    CHECK(at(L"uaro-graphics\\uaro-graphics.ini").find("channel = release") == std::string::npos);
    put(L"uaro-graphics\\version.txt", "3.0.0 build 2026-10-24 03:00\r\n");
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    CHECK(at(L"uaro-graphics\\uaro-graphics.ini").find("channel = beta") != std::string::npos);
    CHECK(at(L"uaro-graphics\\uaro-graphics.ini").rfind("[display]\r\nmode = window\r\n", 0) == 0);
}

TEST(finish_install_writes_the_apps_entry) {
    client();
    put(L"uaro-graphics\\version.txt", "2.10.0 build 2026-10-22 18:00\r\n");
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    CHECK(fake::windows().apps.size() == 1);
    std::map<std::wstring, std::wstring>& e = fake::windows().apps[L"uaRO Graphics"];
    CHECK(e[L"DisplayName"] == L"uaRO Graphics");
    CHECK(e[L"DisplayVersion"] == L"2.10.0");
    CHECK(e[L"UninstallString"] == L"\"" + app::exeFolder() + L"\\uaro-graphics.exe\" --uninstall");
    CHECK(e[L"InstallLocation"] == app::exeFolder());
}

// 2.10: the removal - uninstall.bat's work, and more: nothing of ours is left unless the player keeps it.
TEST(uninstall_leaves_the_folder_as_before_the_install) {
    client();
    put(L"ddraw.dll", "dgVoodoo DDraw");  // Windows finds DDraw.dll by any case; the stand-in by this one
    put(L"D3DImm.dll", "dgVoodoo D3DImm");
    put(L"dgVoodoo.conf", "[General]");
    const std::map<std::wstring, std::string> files = filesAnyCase();
    const std::set<std::wstring> folders = foldersAnyCase();
    installedAndPlayed();
    const app::installer::Result r = app::installer::uninstall(app::exeFolder(), false);
    CHECK(r.ok);
    CHECK(filesAnyCase() == files);
    CHECK(foldersAnyCase() == folders);
}

TEST(uninstall_keeps_the_settings_and_the_pack) {
    client();
    installedAndPlayed();
    put(L"uaro-graphics\\uaro-pad-binds.ini", "the characters' layouts");
    put(L"uaro-graphics\\textures\\uaro-textures.zip", "the pack");
    put(L"uaro-graphics\\backup\\2026-10-13-120000\\uaro-graphics.ini", "a copy");
    CHECK(app::installer::uninstall(app::exeFolder(), true).ok);
    CHECK_EQ(at(L"uaro-graphics\\uaro-graphics.ini"), std::string("; the template\r\n"));
    CHECK_EQ(at(L"uaro-graphics\\uaro-pad-binds.ini"), std::string("the characters' layouts"));
    CHECK_EQ(at(L"uaro-graphics\\textures\\uaro-textures.zip"), std::string("the pack"));
    CHECK_EQ(at(L"uaro-graphics\\backup\\2026-10-13-120000\\uaro-graphics.ini"), std::string("a copy"));
    CHECK_EQ(at(L"uaro-graphics\\logs\\uaro-graphics.log"), std::string("<none>"));
    CHECK_EQ(at(L"uaro-graphics\\ddraw.dll"), std::string("<none>"));
    CHECK_EQ(at(L"ddraw.dll"), std::string("<none>"));
    CHECK_EQ(at(L"uaro-graphics.exe"), std::string("<none>"));
}

TEST(uninstall_removes_the_apps_entry) {
    client();
    installedAndPlayed();
    CHECK(!fake::windows().apps.empty());
    CHECK(app::installer::uninstall(app::exeFolder(), false).ok);
    CHECK(fake::windows().apps.empty());
}

namespace {

const std::wstring kDesktop = L"C:\\Users\\player\\Desktop";  // the stand-ins' desktop (app/shortcuts)

void onDesktop(const std::wstring& name, const std::wstring& target) {
    fake::Windows& w = fake::windows();
    for (const wchar_t* f : {L"C:\\Users", L"C:\\Users\\player", L"C:\\Users\\player\\Desktop"}) w.folders.insert(f);
    w.files[kDesktop + L"\\" + name] = "lnk:" + utf8(target);
}

std::string desktop(const std::wstring& name) {
    const auto it = fake::windows().files.find(kDesktop + L"\\" + name);
    return it == fake::windows().files.end() ? "<none>" : it->second;
}

}  // namespace

// 2.11: the desktop shortcut «uaRO» opens the launcher «Гра» (decision 16): the game's own shortcuts go aside into
// uaro-graphics\shortcuts and come back on removal.
TEST(shortcuts_of_the_game_go_aside_and_come_back) {
    client();
    onDesktop(L"uaRO Online.lnk", fake::path(L"uaRO.exe"));
    onDesktop(L"Ragnarok.lnk", L"c:\\uaro\\UARO.EXE");  // the same game, another case
    onDesktop(L"Discord.lnk", L"C:\\Discord\\Discord.exe");
    CHECK(app::installer::putPackage(app::exeFolder(), stage("1.0.0"), false).ok);
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    CHECK_EQ(desktop(L"uaRO.lnk"), "lnk:" + utf8(fake::path(L"uaro-graphics.exe")));
    CHECK_EQ(desktop(L"uaRO Online.lnk"), std::string("<none>"));
    CHECK_EQ(desktop(L"Ragnarok.lnk"), std::string("<none>"));
    CHECK_EQ(at(L"uaro-graphics\\shortcuts\\uaRO Online.lnk"), "lnk:" + utf8(fake::path(L"uaRO.exe")));
    CHECK_EQ(desktop(L"Discord.lnk"), std::string("lnk:C:\\Discord\\Discord.exe"));
    CHECK(app::installer::uninstall(app::exeFolder(), false).ok);
    CHECK_EQ(desktop(L"uaRO.lnk"), std::string("<none>"));
    CHECK_EQ(desktop(L"uaRO Online.lnk"), "lnk:" + utf8(fake::path(L"uaRO.exe")));
    CHECK_EQ(desktop(L"Ragnarok.lnk"), std::string("lnk:c:\\uaro\\UARO.EXE"));
    CHECK_EQ(desktop(L"Discord.lnk"), std::string("lnk:C:\\Discord\\Discord.exe"));
}

TEST(the_shortcut_is_made_once) {
    client();
    onDesktop(L"Discord.lnk", L"C:\\Discord\\Discord.exe");
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    CHECK(desktop(L"uaRO.lnk") != "<none>");
    fake::windows().files.erase(kDesktop + L"\\uaRO.lnk");  // the player deleted it
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);  // the next update
    CHECK_EQ(desktop(L"uaRO.lnk"), std::string("<none>"));
}

namespace {

// A file anywhere, its folders made
void putAt(const std::wstring& full, const std::string& bytes) {
    fake::Windows& w = fake::windows();
    for (size_t at = full.find(L'\\'); at != std::wstring::npos; at = full.find(L'\\', at + 1))
        w.folders.insert(full.substr(0, at));
    w.files[full] = bytes;
}

const std::wstring kHd = L"C:\\Games\\uaRO-HD";

// «uaRO HD», the full client, next to the old one: its build's uaro-client.json names it
void hdClient() {
    putAt(kHd + L"\\uaRO.exe", "MZ uaRO HD");
    putAt(kHd + L"\\uaro-graphics.exe", "uaRO Graphics exe 3.0.1");
    putAt(kHd + L"\\uaro-graphics\\version.txt", "3.0.1 build 2026-10-23 18:00\r\n");
    putAt(kHd + L"\\uaro-client.json", "{\"format\":1,\"client\":\"uaRO HD\",\"version\":\"2026.10.6\",\"files\":[]}");
}

}  // namespace

// 3.0.1: the old uaRO and «uaRO HD» side by side - each its shortcut and its Apps entry, neither takes the other's
TEST(two_clients_keep_their_own_names) {
    client();
    put(L"uaro-graphics.exe", "uaRO Graphics exe 3.0.1");
    put(L"uaro-graphics\\version.txt", "3.0.1 build 2026-10-23 18:00\r\n");
    hdClient();
    onDesktop(L"Discord.lnk", L"C:\\Discord\\Discord.exe");
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    CHECK(app::installer::finishInstall(kHd, true).ok);
    CHECK_EQ(desktop(L"uaRO.lnk"), "lnk:" + utf8(fake::path(L"uaro-graphics.exe")));
    CHECK_EQ(desktop(L"uaRO HD.lnk"), "lnk:" + utf8(kHd + L"\\uaro-graphics.exe"));
    auto& apps = fake::windows().apps;
    CHECK(apps.size() == 2);
    CHECK(apps[L"uaRO Graphics"][L"InstallLocation"] == app::exeFolder());
    CHECK(apps[L"uaRO Graphics (uaRO HD)"][L"InstallLocation"] == kHd);
    CHECK(apps[L"uaRO Graphics (uaRO HD)"][L"DisplayName"] == L"uaRO Graphics (uaRO HD)");
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);  // the old client's next update
    CHECK(apps[L"uaRO Graphics (uaRO HD)"][L"InstallLocation"] == kHd);
    CHECK(app::installer::uninstall(kHd, false).ok);
    CHECK(apps.count(L"uaRO Graphics (uaRO HD)") == 0);
    CHECK(apps[L"uaRO Graphics"][L"InstallLocation"] == app::exeFolder());
    CHECK_EQ(desktop(L"uaRO HD.lnk"), std::string("<none>"));
    CHECK_EQ(desktop(L"uaRO.lnk"), "lnk:" + utf8(fake::path(L"uaro-graphics.exe")));
}

// 3.0.1: a second copy of the same client: «uaRO (2)», and the Apps entry stays the first one's while it is there
TEST(a_second_copy_does_not_take_the_first_ones_names) {
    client();
    put(L"uaro-graphics.exe", "uaRO Graphics exe 3.0.1");
    onDesktop(L"Discord.lnk", L"C:\\Discord\\Discord.exe");
    CHECK(app::installer::finishInstall(app::exeFolder(), true).ok);
    const std::wstring copy = L"C:\\Games\\uaRO-copy";  // no uaro-client.json: the old client again
    putAt(copy + L"\\uaRO.exe", "MZ uaRO");
    putAt(copy + L"\\uaro-graphics.exe", "uaRO Graphics exe 3.0.1");
    CHECK(app::installer::finishInstall(copy, true).ok);
    CHECK_EQ(desktop(L"uaRO.lnk"), "lnk:" + utf8(fake::path(L"uaro-graphics.exe")));
    CHECK_EQ(desktop(L"uaRO (2).lnk"), "lnk:" + utf8(copy + L"\\uaro-graphics.exe"));
    auto& apps = fake::windows().apps;
    CHECK(apps.size() == 1 && apps[L"uaRO Graphics"][L"InstallLocation"] == app::exeFolder());
    fake::windows().files.erase(fake::path(L"uaro-graphics.exe"));  // the first copy deleted by hand
    app::installer::atStart(copy);
    CHECK(apps.size() == 1 && apps[L"uaRO Graphics"][L"InstallLocation"] == copy);  // its stale entry taken over
}

// 3.0.1: 3.0.0 named every client's entry «uaRO Graphics» - the HD copy's start renames its own
TEST(the_start_renames_an_entry_of_the_old_name) {
    fake::reset();
    hdClient();
    putAt(kHd + L"\\uaro-graphics\\shortcuts\\made.txt", "uaRO.lnk\r\n");  // 3.0.0 made its shortcut once
    app::writeAppsEntry(L"uaRO Graphics", kHd, "3.0.0");
    app::installer::atStart(kHd);
    auto& apps = fake::windows().apps;
    CHECK(apps.count(L"uaRO Graphics") == 0);
    CHECK(apps[L"uaRO Graphics (uaRO HD)"][L"InstallLocation"] == kHd);
    CHECK(apps[L"uaRO Graphics (uaRO HD)"][L"DisplayVersion"] == L"3.0.1");
    CHECK_EQ(desktop(L"uaRO HD.lnk"), std::string("<none>"));  // made once already
}

// 3.0.1: the full client is unpacked, not installed - its first start makes the shortcut, once
TEST(an_unpacked_client_gets_its_shortcut_at_the_start) {
    fake::reset();
    hdClient();
    onDesktop(L"Discord.lnk", L"C:\\Discord\\Discord.exe");
    app::installer::atStart(kHd);
    CHECK_EQ(desktop(L"uaRO HD.lnk"), "lnk:" + utf8(kHd + L"\\uaro-graphics.exe"));
    CHECK(fake::windows().apps.count(L"uaRO Graphics (uaRO HD)") == 1);
    fake::windows().files.erase(kDesktop + L"\\uaRO HD.lnk");  // the player deleted it: not made again
    app::installer::atStart(kHd);
    CHECK_EQ(desktop(L"uaRO HD.lnk"), std::string("<none>"));
    const std::wstring stray = L"C:\\Games\\not-a-game";  // no game in the folder: nothing made
    putAt(stray + L"\\uaro-graphics.exe", "uaRO Graphics exe 3.0.1");
    app::installer::atStart(stray);
    CHECK(fake::windows().apps.size() == 1);
}
