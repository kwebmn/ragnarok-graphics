#include <windows.h>

#include "app/reports.hpp"
#include "app/reports_window.hpp"

namespace uagfx::app {
namespace {

volatile LONG g_unread = -1;
std::wstring* g_dir = nullptr;  // the thread's own copy, never freed while it may run

DWORD WINAPI ask(void*) {
    sendQueued(*g_dir);
    std::vector<reports::Summary> list;
    std::string error;
    if (listReports(computerKey(*g_dir), list, error)) InterlockedExchange(&g_unread, reports::unreadOf(list));
    return 0;
}

}  // namespace

void startReportsCheck(const std::wstring& dir) {
    if (g_dir) return;  // once a run
    g_dir = new std::wstring(dir);
    HANDLE t = CreateThread(nullptr, 0, ask, nullptr, 0, nullptr);
    if (t) CloseHandle(t);
}

int reportsUnread() { return int(InterlockedCompareExchange(&g_unread, 0, 0)); }

}  // namespace uagfx::app
