#include <windows.h>

#include "app/check_notify.hpp"
#include "app/reports.hpp"
#include "app/reports_window.hpp"

namespace uagfx::app {
namespace {

volatile LONG g_unread = -1;
// 3.0.9-beta.4: put by the thread, never freed (3.0.10-beta.3: one a check - a few an hour)
std::vector<reports::Answer>* volatile g_answers = nullptr;
volatile LONG g_round = 0;     // 3.0.10-beta.3: the checks done (the start's, then one every 5 minutes)
volatile LONG g_busy = 0;      // one asking at a time
volatile LONG g_reports = -1;  // the reports the last check found (-1 unknown): none - no question again
std::wstring* g_dir = nullptr;  // the thread's own copies, never freed while it may run
std::string* g_key = nullptr;

DWORD WINAPI ask(void*) {
    sendQueued(*g_dir);
    std::vector<reports::Summary> list;
    std::string error;
    if (listReports(*g_key, list, error)) {
        noteAnswersOf(*g_dir, list);  // 3.0.9-beta.4: the game's card from it too
        InterlockedExchangePointer(reinterpret_cast<PVOID volatile*>(&g_answers),
                                   new std::vector<reports::Answer>(reports::answersOf(list)));
        InterlockedExchange(&g_unread, reports::unreadOf(list));  // after the answers: the window reads them then
        InterlockedExchange(&g_reports, LONG(list.size()));
    }
    InterlockedIncrement(&g_round);  // after all of it: the window takes the round's answer
    InterlockedExchange(&g_busy, 0);
    checkDone();
    return 0;
}

void run() {
    if (InterlockedCompareExchange(&g_busy, 1, 0) != 0) return;  // one asks already
    HANDLE t = CreateThread(nullptr, 0, ask, nullptr, 0, nullptr);
    if (t)
        CloseHandle(t);
    else
        InterlockedExchange(&g_busy, 0);
}

}  // namespace

void startReportsCheck(const std::wstring& dir, const std::string& key) {
    if (g_dir) return;  // once a run
    g_dir = new std::wstring(dir);
    g_key = new std::string(key);
    run();
}

void recheckReports() {
    // 3.0.10-beta.3: only a computer with reports (or not known: the site did not answer the start's check)
    if (g_dir && InterlockedCompareExchange(&g_reports, 0, 0) != 0) run();
}

void reportsKnown(int count) {
    if (count > 0) InterlockedExchange(&g_reports, LONG(count));  // a first report sent in «Звернення» meanwhile
}

int reportsRound() { return int(InterlockedCompareExchange(&g_round, 0, 0)); }

int reportsUnread() { return int(InterlockedCompareExchange(&g_unread, 0, 0)); }

std::vector<reports::Answer> reportsAnswers() {
    const auto* a = static_cast<const std::vector<reports::Answer>*>(
        InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(&g_answers), nullptr, nullptr));
    return a ? *a : std::vector<reports::Answer>{};
}

}  // namespace uagfx::app
