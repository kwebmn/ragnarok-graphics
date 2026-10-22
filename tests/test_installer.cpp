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
