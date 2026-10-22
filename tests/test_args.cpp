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

TEST(window_language_follows_windows_until_one_is_chosen) {
    CHECK(settings::windowLang(std::nullopt, 0x22) == settings::Lang::Uk);  // LANG_UKRAINIAN
    CHECK(settings::windowLang(std::nullopt, 0x19) == settings::Lang::Ru);  // LANG_RUSSIAN
    CHECK(settings::windowLang(std::nullopt, 0x09) == settings::Lang::En);
    CHECK(settings::windowLang(settings::Lang::Ru, 0x22) == settings::Lang::Ru);  // the app's choice first
}
