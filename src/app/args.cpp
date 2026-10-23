#include "app/args.hpp"

#include <cwchar>
#include <vector>

#include "core/log_files.hpp"

namespace uagfx::app {
namespace {

// The words before " -- ": a word in quotes keeps its spaces.
std::vector<std::wstring> words(const std::wstring& line) {
    std::vector<std::wstring> out;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && line[i] == L' ') ++i;
        if (i >= line.size()) break;
        std::wstring w;
        if (line[i] == L'"') {
            const size_t end = line.find(L'"', i + 1);
            w = line.substr(i + 1, end == std::wstring::npos ? std::wstring::npos : end - i - 1);
            i = end == std::wstring::npos ? line.size() : end + 1;
        } else {
            const size_t end = line.find(L' ', i);
            w = line.substr(i, end == std::wstring::npos ? std::wstring::npos : end - i);
            i = end == std::wstring::npos ? line.size() : end;
        }
        out.push_back(w);
    }
    return out;
}

unsigned long number(const std::wstring& w) { return std::wcstoul(w.c_str(), nullptr, 10); }

}  // namespace

Args parseArgs(const std::wstring& line) {
    Args a;
    std::wstring head = line;
    const size_t rest = line == L"--" || line.rfind(L"-- ", 0) == 0 ? 0 : line.find(L" -- ");
    if (rest != std::wstring::npos) {
        const size_t from = rest == 0 ? 3 : rest + 4;
        a.gameArgs = from < line.size() ? line.substr(from) : L"";
        head = line.substr(0, rest);
    } else if (line.size() >= 3 && line.compare(line.size() - 3, 3, L" --") == 0) {
        head = line.substr(0, line.size() - 3);
    }
    const std::vector<std::wstring> w = words(head);
    for (size_t i = 0; i < w.size(); ++i) {
        const std::wstring& k = w[i];
        const std::wstring next = i + 1 < w.size() ? w[i + 1] : L"";
        if (k == L"--watch") {
            a.mode = Args::Mode::Watch;
            a.pid = number(next);
            ++i;
        } else if (k == L"--ask-update") {
            a.mode = Args::Mode::AskUpdate;
            a.pid = number(next);
            ++i;
        } else if (k == L"--install") {
            a.mode = Args::Mode::Install;
        } else if (k == L"--update") {
            a.mode = Args::Mode::Update;
        } else if (k == L"--rollback") {
            a.mode = Args::Mode::Rollback;
        } else if (k == L"--repair") {
            a.mode = Args::Mode::Repair;
        } else if (k == L"--uninstall") {
            a.mode = Args::Mode::Uninstall;
        } else if (k == L"--uninstall-run") {
            a.mode = Args::Mode::UninstallRun;
        } else if (k == L"--setup") {
            a.mode = Args::Mode::Setup;
        } else if (k == L"--quiet" || k == L"/quiet") {
            a.quiet = true;
        } else if (k == L"--after") {
            a.pid = number(next);
            ++i;
        } else if (k == L"--then-app") {
            a.then = Args::Then::App;
        } else if (k == L"--then-game") {
            a.then = Args::Then::Game;
        } else if (k == L"--keep") {
            a.keep = next != L"0";
            ++i;
        } else if (k == L"--dir") {
            a.dir = next;
            ++i;
        } else if (k == L"--report") {
            a.report = true;
            if (!next.empty() && next.rfind(L"--", 0) != 0) {  // its shot, when one follows
                a.reportShot = next;
                ++i;
            }
        } else if (k == L"--map") {
            a.reportMap = next;
            ++i;
        } else if (k == L"--log-slot") {
            const unsigned long slot = number(next);
            a.logSlot = slot >= 1 && slot <= unsigned(logfiles::kSlots) ? int(slot) : 1;
            ++i;
        }
    }
    return a;
}

std::wstring argsLine(const Args& a) {
    std::wstring s;
    const std::wstring pid = std::to_wstring(a.pid);
    switch (a.mode) {
        case Args::Mode::Settings: break;
        case Args::Mode::Watch: s = L"--watch " + pid; break;
        case Args::Mode::AskUpdate: s = L"--ask-update " + pid; break;
        case Args::Mode::Install: s = L"--install"; break;
        case Args::Mode::Update: s = L"--update"; break;
        case Args::Mode::Rollback: s = L"--rollback"; break;
        case Args::Mode::Repair: s = L"--repair"; break;
        case Args::Mode::Uninstall: s = L"--uninstall"; break;
        case Args::Mode::UninstallRun: s = L"--uninstall-run"; break;
        case Args::Mode::Setup: s = L"--setup"; break;
    }
    if (a.quiet) s += L" --quiet";
    if (!a.dir.empty()) s += L" --dir \"" + a.dir + L"\"";
    if (!a.keep) s += L" --keep 0";
    if (a.pid && a.mode != Args::Mode::Watch && a.mode != Args::Mode::AskUpdate) s += L" --after " + pid;
    if (a.then == Args::Then::App) s += L" --then-app";
    if (a.then == Args::Then::Game) s += L" --then-game";
    if (a.report) s += a.reportShot.empty() ? L" --report" : L" --report \"" + a.reportShot + L"\"";
    if (!a.reportMap.empty()) s += L" --map " + a.reportMap;
    if (a.logSlot > 1) s += L" --log-slot " + std::to_wstring(a.logSlot);
    if (a.then == Args::Then::Game || !a.gameArgs.empty()) s += L" -- " + a.gameArgs;
    return s;
}

}  // namespace uagfx::app
