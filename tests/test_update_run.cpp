// 2.10: updates from uaro.kiev.ua (app/update_run) over the stand-ins of tests/fake_windows.cpp: the site is
// fake::Windows::web.
#include <string>

#include "app/files.hpp"
#include "app/installer.hpp"
#include "app/update_run.hpp"
#include "core/sha256.hpp"
#include "fake_windows.hpp"
#include "test.hpp"
#include "zip_fixture.hpp"

using namespace uagfx;

namespace {

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

std::string package(const std::string& v) {
    return fixture::zipBytes({fixture::item("uaro-graphics/ddraw.dll", "uaRO Graphics ddraw " + v, true),
                              fixture::item("uaro-graphics/uaro-graphics.exe", "uaRO Graphics exe " + v, true),
                              fixture::item("uaro-graphics/version.txt", v + " build 2026-10-22 18:00\r\n"),
                              fixture::item("install.bat", "rem uaRO Graphics: the older update.bat's way in")});
}

std::string section(const std::string& name, const std::string& file, const std::string& bytes,
                    const std::string& version = "") {
    return "[" + name + "]\r\n" + (version.empty() ? "" : "version = " + version + "\r\n") + "file = " + file +
           "\r\nsize = " + std::to_string(bytes.size()) + "\r\nsha256 = " + sha256::hexOf(bytes) + "\r\n";
}

// The site with release `v` (its zip in web), its update.ini parsed.
update::Manifest site(const std::string& v, const std::string& dlss = "") {
    fake::Windows& w = fake::windows();
    const std::string file = "uaro-graphics-" + v + ".zip";
    w.web[file] = package(v);
    std::string text = "[release]\r\nformat = 1\r\nversion = " + v + "\r\n" + section("package", file, w.web[file]);
    if (!dlss.empty()) {
        w.web["uaro-graphics-dlss-streamline-2.14.1.zip"] = dlss;
        text += section("dlss", "uaro-graphics-dlss-streamline-2.14.1.zip", dlss, "streamline 2.14.1");
    }
    w.web["update.ini"] = text;
    update::Manifest m;
    std::string error;
    CHECK(update::parseManifest(text, m, error));
    return m;
}

// A client with 1.0.0 installed and its settings.
fake::Windows& installed() {
    fake::Windows& w = fake::reset();
    put(L"uaRO.exe", "MZ uaRO");
    put(L"uaro-graphics\\update\\files\\uaro-graphics\\ddraw.dll", "uaRO Graphics ddraw 1.0.0");
    put(L"uaro-graphics\\update\\files\\uaro-graphics\\uaro-graphics.exe", "uaRO Graphics exe 1.0.0");
    put(L"uaro-graphics\\update\\files\\uaro-graphics\\version.txt", "1.0.0 build 2026-10-20 10:00\r\n");
    CHECK(app::installer::putPackage(app::exeFolder(), fake::path(L"uaro-graphics\\update\\files"), false).ok);
    w.files.erase(fake::path(L"uaro-graphics\\update\\files\\install.bat"));
    put(L"uaro-graphics\\uaro-graphics.ini", "[display]\r\nmode = window\r\n");
    return w;
}

const app::Report kGo = [](const app::Step&) { return true; };

}  // namespace

TEST(update_check_offers_the_sites_version_and_keeps_skip) {
    installed();
    site("2.0.0");
    put(L"uaro-graphics\\update.ini", "[release]\r\nformat = 1\r\n[local]\r\nskip = 1.5.0\r\n");
    const app::CheckResult c = app::checkNow(app::exeFolder());
    CHECK(c.ok);
    CHECK_EQ(c.offer, std::string("2.0.0"));
    CHECK_EQ(app::skipped(app::exeFolder()), std::string("1.5.0"));
    CHECK(at(L"uaro-graphics\\update.ini").rfind(fake::windows().web["update.ini"], 0) == 0);  // the site's text first
    app::setSkipped(app::exeFolder(), "2.0.0");
    CHECK_EQ(app::skipped(app::exeFolder()), std::string("2.0.0"));
    CHECK_EQ(app::checkNow(app::exeFolder()).offer, std::string(""));  // rolled back from it: not offered again
    fake::windows().web["update.ini"] = "<html>502</html>";
    CHECK(!app::checkNow(app::exeFolder()).ok);
}

TEST(update_downloads_checks_and_installs) {
    installed();
    std::wstring error;
    CHECK(app::runUpdate(app::exeFolder(), site("2.0.0"), false, kGo, error));
    CHECK_EQ(at(L"uaro-graphics\\version.txt"), std::string("2.0.0 build 2026-10-22 18:00\r\n"));
    CHECK_EQ(at(L"ddraw.dll"), std::string("uaRO Graphics ddraw 2.0.0"));
    CHECK_EQ(app::installer::previousVersion(app::exeFolder()), std::string("1.0.0"));
    CHECK_EQ(at(L"uaro-graphics\\uaro-graphics.ini"), std::string("[display]\r\nmode = window\r\n"));
    CHECK(fake::windows().folders.count(fake::path(L"uaro-graphics\\update")) == 0);
    CHECK_EQ(at(L"install.bat"), std::string("<none>"));
}

TEST(update_refuses_a_package_that_is_not_the_sites) {
    fake::Windows& w = installed();
    const update::Manifest m = site("2.0.0");
    w.web[m.package.file][40] ^= 1;  // a byte changed on the way
    const auto before = w.files;
    std::wstring error;
    CHECK(!app::runUpdate(app::exeFolder(), m, false, kGo, error));
    CHECK(error.find(L"SHA-256") != std::wstring::npos);
    CHECK_EQ(at(L"ddraw.dll"), std::string("uaRO Graphics ddraw 1.0.0"));
    CHECK_EQ(at(L"uaro-graphics\\version.txt"), std::string("1.0.0 build 2026-10-20 10:00\r\n"));
}

TEST(update_cancelled_changes_nothing) {
    installed();
    std::wstring error;
    const app::Report stop = [](const app::Step& s) { return s.what != app::Step::Install; };
    CHECK(!app::runUpdate(app::exeFolder(), site("2.0.0"), false, stop, error));
    CHECK_EQ(at(L"ddraw.dll"), std::string("uaRO Graphics ddraw 1.0.0"));
    CHECK(!app::installer::canRollBack(app::exeFolder()));
}

TEST(update_waits_for_the_game) {
    fake::Windows& w = installed();
    w.gameRunning = true;
    int waits = 0;
    std::wstring error;
    CHECK(app::runUpdate(app::exeFolder(), site("2.0.0"), false,
                         [&](const app::Step& s) {
                             if (s.what == app::Step::Wait) ++waits;
                             return true;
                         },
                         error));
    CHECK(waits >= 1);
    CHECK_EQ(at(L"ddraw.dll"), std::string("uaRO Graphics ddraw 2.0.0"));
}

TEST(update_gets_dlss_on_an_nvidia_card) {
    fake::Windows& w = installed();
    w.system.adapters.push_back(win::AdapterProbe{"NVIDIA GeForce RTX 4070", 0x10de, 12282, true, 0xc100});
    const std::string dlss =
        fixture::zipBytes({fixture::item("uaro-graphics/dlss/sl.interposer.dll", "sl", true),
                           fixture::item("uaro-graphics/dlss/version.txt", "streamline 2.14.1\r\n")});
    std::wstring error;
    CHECK(app::runUpdate(app::exeFolder(), site("2.0.0", dlss), false, kGo, error));
    CHECK_EQ(at(L"uaro-graphics\\dlss\\version.txt"), std::string("streamline 2.14.1\r\n"));
    CHECK_EQ(at(L"uaro-graphics\\dlss\\sl.interposer.dll"), std::string("sl"));
}

TEST(update_leaves_dlss_elsewhere) {
    fake::Windows& w = installed();
    w.system.adapters.push_back(win::AdapterProbe{"AMD Radeon RX 7800 XT", 0x1002, 16368, true, 0xc100});
    const std::string dlss = fixture::zipBytes({fixture::item("uaro-graphics/dlss/sl.interposer.dll", "sl")});
    std::wstring error;
    CHECK(app::runUpdate(app::exeFolder(), site("2.0.0", dlss), false, kGo, error));
    CHECK_EQ(at(L"uaro-graphics\\dlss\\sl.interposer.dll"), std::string("<none>"));
}

TEST(models_come_from_the_site) {
    fake::Windows& w = installed();
    const std::string models =
        fixture::zipBytes({fixture::item("uaro-graphics/ai/4x-gameai-2.0.bin", "weights", true),
                           fixture::item("uaro-graphics/ai/models-version.txt", "gameai-2.0 1")});
    update::Manifest m = site("1.0.0");
    w.web["uaro-graphics-models-gameai-2.0-1.zip"] = models;
    m.models = {"gameai-2.0 1", "uaro-graphics-models-gameai-2.0-1.zip", sha256::hexOf(models), models.size()};
    std::wstring error;
    CHECK(app::runModels(app::exeFolder(), m, kGo, error));
    CHECK_EQ(at(L"uaro-graphics\\ai\\4x-gameai-2.0.bin"), std::string("weights"));
}

// 2.10: the setup (uaro-graphics.exe with the package appended) puts the package it carries in the way an update does.
TEST(setup_installs_the_package_it_carries) {
    fake::Windows& w = fake::reset();
    put(L"uaRO.exe", "MZ uaRO");
    put(L"ddraw.dll", "dgVoodoo DDraw");
    const std::string zip =
        fixture::zipBytes({fixture::item("uaro-graphics/ddraw.dll", "uaRO Graphics ddraw 2.0.0", true),
                           fixture::item("uaro-graphics/uaro-graphics.exe", "uaRO Graphics exe 2.0.0", true),
                           fixture::item("uaro-graphics/version.txt", "2.0.0 build 2026-10-22 18:00\r\n"),
                           fixture::item("uaro-graphics/defaults/uaro-graphics.ini", "; the template\r\n"),
                           fixture::item("install.bat", "rem uaRO Graphics: the older update.bat's way in")});
    std::wstring error;
    CHECK(app::runInstall(app::exeFolder(), zip, kGo, error));
    CHECK_EQ(at(L"ddraw.dll"), std::string("uaRO Graphics ddraw 2.0.0"));
    CHECK_EQ(at(L"uaro-graphics.exe"), std::string("uaRO Graphics exe 2.0.0"));
    CHECK_EQ(at(L"uaro-graphics\\dgvoodoo\\DDraw.dll"), std::string("dgVoodoo DDraw"));
    CHECK_EQ(at(L"uaro-graphics\\uaro-graphics.ini"), std::string("; the template\r\n"));
    CHECK(w.appsEntry[L"DisplayVersion"] == L"2.0.0");
    CHECK_EQ(at(L"install.bat"), std::string("<none>"));
    CHECK(w.folders.count(fake::path(L"uaro-graphics\\update")) == 0);
    std::wstring broken;
    CHECK(!app::runInstall(app::exeFolder(), zip.substr(0, zip.size() / 2), kGo, broken));
}
