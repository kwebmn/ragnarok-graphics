// 2.10: the settings app's modes on its command line (app/args) - the update, rollback, repair, removal and install
// run as processes of their own.
#include <string>

#include "app/args.hpp"
#include "core/settings/strings.hpp"
#include "test.hpp"

using namespace uagfx;
using app::Args;

TEST(args_read_every_mode) {
    CHECK(app::parseArgs(L"").mode == Args::Mode::Settings);
    const Args w = app::parseArgs(L"--watch 1234");
    CHECK(w.mode == Args::Mode::Watch && w.pid == 1234);
    const Args i = app::parseArgs(L"--install /quiet");  // install.bat passes on update.bat's /quiet
    CHECK(i.mode == Args::Mode::Install && i.quiet);
    CHECK(app::parseArgs(L"--install --quiet").quiet && !app::parseArgs(L"--install").quiet);
    const Args u = app::parseArgs(L"--update --after 77 --then-game -- 1rag1 \"x y\"");
    CHECK(u.mode == Args::Mode::Update && u.pid == 77 && u.then == Args::Then::Game);
    CHECK(u.gameArgs == L"1rag1 \"x y\"");
    const Args a = app::parseArgs(L"--ask-update 5 -- ");
    CHECK(a.mode == Args::Mode::AskUpdate && a.pid == 5 && a.gameArgs.empty());
    CHECK(app::parseArgs(L"--rollback --then-app").then == Args::Then::App);
    CHECK(app::parseArgs(L"--repair").mode == Args::Mode::Repair);
    CHECK(app::parseArgs(L"--uninstall").mode == Args::Mode::Uninstall);
    const Args r = app::parseArgs(L"--uninstall-run --dir \"C:\\Games\\ua RO\" --keep 0 --after 9");
    CHECK(r.mode == Args::Mode::UninstallRun && r.dir == L"C:\\Games\\ua RO" && !r.keep && r.pid == 9);
}

TEST(args_round_trip) {
    Args u;
    u.mode = Args::Mode::Update;
    u.pid = 4242;
    u.then = Args::Then::Game;
    u.gameArgs = L"1rag1 \"two words\"";
    Args r;
    r.mode = Args::Mode::UninstallRun;
    r.dir = L"D:\\Ігри\\uaRO";
    r.keep = false;
    r.pid = 7;
    Args i;
    i.mode = Args::Mode::Install;
    i.quiet = true;
    for (const Args& a : {u, r, i}) {
        const Args b = app::parseArgs(app::argsLine(a));
        CHECK(b.mode == a.mode && b.pid == a.pid && b.quiet == a.quiet && b.then == a.then && b.keep == a.keep);
        CHECK(b.dir == a.dir && b.gameArgs == a.gameArgs);
    }
}

TEST(args_read_a_report_from_the_game) {
    // 2.13: the game's Ctrl+Shift+F10 - «Нове звернення» with its shot and the map
    const Args r =
        app::parseArgs(L"--report \"C:\\Ігри\\ua RO\\ScreenShot\\uaro-20261022-215701.png\" --map prt_fild08");
    CHECK(r.mode == Args::Mode::Settings && r.report);
    CHECK(r.reportShot == L"C:\\Ігри\\ua RO\\ScreenShot\\uaro-20261022-215701.png");
    CHECK(r.reportMap == L"prt_fild08");
    const Args bare = app::parseArgs(L"--report --map payon");  // no shot: the map is no shot's name
    CHECK(bare.report && bare.reportShot.empty() && bare.reportMap == L"payon");
    CHECK(!app::parseArgs(L"").report);
    Args a;
    a.report = true;
    a.reportShot = L"D:\\a b\\shot.png";
    a.reportMap = L"payon";
    const Args b = app::parseArgs(app::argsLine(a));
    CHECK(b.mode == Args::Mode::Settings && b.report && b.reportShot == a.reportShot && b.reportMap == a.reportMap);
}

// 3.0.3-beta.1: the game's window (its logs' number) to the watcher and to «Нове звернення»
TEST(args_carry_the_game_windows_log_slot) {
    CHECK(app::parseArgs(L"--watch 12").logSlot == 1);
    const Args w = app::parseArgs(L"--watch 12 --log-slot 3");
    CHECK(w.mode == Args::Mode::Watch && w.pid == 12 && w.logSlot == 3);
    const Args r = app::parseArgs(L"--report \"C:\\s.png\" --map payon --log-slot 2");
    CHECK(r.report && r.reportShot == L"C:\\s.png" && r.reportMap == L"payon" && r.logSlot == 2);
    CHECK(app::parseArgs(L"--watch 5 --log-slot 0").logSlot == 1);   // out of 1-8: the first
    CHECK(app::parseArgs(L"--watch 5 --log-slot 99").logSlot == 1);
    Args a;
    a.mode = Args::Mode::Watch;
    a.pid = 77;
    CHECK(app::argsLine(a).find(L"--log-slot") == std::wstring::npos);  // the first window's: as before
    a.logSlot = 4;
    const Args b = app::parseArgs(app::argsLine(a));
    CHECK(b.mode == Args::Mode::Watch && b.pid == 77 && b.logSlot == 4);
}

TEST(window_language_follows_windows_until_one_is_chosen) {
    CHECK(settings::windowLang(std::nullopt, 0x22) == settings::Lang::Uk);  // LANG_UKRAINIAN
    CHECK(settings::windowLang(std::nullopt, 0x19) == settings::Lang::Ru);  // LANG_RUSSIAN
    CHECK(settings::windowLang(std::nullopt, 0x09) == settings::Lang::En);
    CHECK(settings::windowLang(settings::Lang::Ru, 0x22) == settings::Lang::Ru);  // the app's choice first
}

// 3.0.5-beta.6: «Тест графіки»'s end - the settings app sends its table without a window, for that game window's logs
TEST(args_read_the_graphics_tests_report) {
    const Args t = app::parseArgs(L"--test-report \"C:\\Ігри\\ua RO\\uaro-graphics\\graphics-test.txt\" --log-slot 2");
    CHECK(t.mode == Args::Mode::TestReport);
    CHECK(t.testFile == L"C:\\Ігри\\ua RO\\uaro-graphics\\graphics-test.txt");
    CHECK_EQ(t.logSlot, 2);
    CHECK(app::parseArgs(app::argsLine(t)).mode == Args::Mode::TestReport);
    CHECK(app::parseArgs(app::argsLine(t)).testFile == t.testFile);
    CHECK(app::argsLine(t) == L"--test-report \"C:\\Ігри\\ua RO\\uaro-graphics\\graphics-test.txt\" --log-slot 2");
}
