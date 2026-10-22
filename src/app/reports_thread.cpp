#include <windows.h>

#include "app/reports.hpp"
#include "app/reports_window.hpp"

namespace uagfx::app {
namespace {

volatile LONG g_unread = -1;
std::wstring* g_dir = nullptr;  // the thread's own copies, never freed while it may run
std::string* g_key = nullptr;

DWORD WINAPI ask(void*) {
    sendQueued(*g_dir);
    std::vector<reports::Summary> list;
    std::string error;
    if (listReports(*g_key, list, error)) InterlockedExchange(&g_unread, reports::unreadOf(list));
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

}  // namespace uagfx::app
