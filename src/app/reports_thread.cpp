#include <windows.h>

#include "app/check_notify.hpp"
#include "app/reports.hpp"
#include "app/reports_window.hpp"

namespace uagfx::app {
namespace {

volatile LONG g_unread = -1;
std::vector<reports::Answer>* volatile g_answers = nullptr;  // 3.0.9-beta.4: put once by the thread, never freed
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
    }
    checkDone();
    return 0;
}

}  // namespace

void startReportsCheck(const std::wstring& dir, const std::string& key) {
    if (g_dir) return;  // once a run
    g_dir = new std::wstring(dir);
    g_key = new std::string(key);
    HANDLE t = CreateThread(nullptr, 0, ask, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
}

int reportsUnread() { return int(InterlockedCompareExchange(&g_unread, 0, 0)); }

std::vector<reports::Answer> reportsAnswers() {
    const auto* a = static_cast<const std::vector<reports::Answer>*>(
        InterlockedCompareExchangePointer(reinterpret_cast<PVOID volatile*>(&g_answers), nullptr, nullptr));
    return a ? *a : std::vector<reports::Answer>{};
}

}  // namespace uagfx::app
