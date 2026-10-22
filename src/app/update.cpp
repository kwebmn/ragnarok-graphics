#include "app/update.hpp"

#include <windows.h>
#include <shellapi.h>

#include "app/files.hpp"
#include "app/text.hpp"
#include "app/update_run.hpp"

namespace uagfx::app {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
UpdateState g_state;

void set(const UpdateState& s) {
    AcquireSRWLockExclusive(&g_lock);
    g_state = s;
    ReleaseSRWLockExclusive(&g_lock);
}

DWORD WINAPI check(void*) {
    const CheckResult c = checkNow(exeFolder());
    UpdateState s;
    s.kind = c.ok ? UpdateState::Done : UpdateState::Failed;
    s.latest = c.site.package.version;
    s.offer = c.offer;
    s.site = c.site;
    s.error = c.error;
    set(s);
    return 0;
}

}  // namespace

void startUpdateCheck() {
    if (updateState().kind == UpdateState::Checking) return;
    UpdateState s;
    s.kind = UpdateState::Checking;
    set(s);
    HANDLE t = CreateThread(nullptr, 0, check, nullptr, 0, nullptr);
    if (t) {
        CloseHandle(t);
        return;
    }
    s.kind = UpdateState::Failed;
    s.error = "no thread for it";
    set(s);
}

UpdateState updateState() {
    AcquireSRWLockShared(&g_lock);
    const UpdateState s = g_state;
    ReleaseSRWLockShared(&g_lock);
    return s;
}

namespace {

// A .bat of the game's folder this app carries (2.5.27: update.bat's way for get-models.bat too), started.
std::string startBat(const std::wstring& gameDir, const wchar_t* file, const wchar_t* resource) {
    const std::string name = utf8(file);
    const std::wstring bat = gameDir + L"\\" + file;
    {
        // 1.3.1: a client copied without it - the one this app carries goes there first. 1.5.11: always, over an
        // older one too - a fix of it (curl's certificate check) reaches the players with this app, not one update
        // late.
        HRSRC r = FindResourceW(nullptr, resource, MAKEINTRESOURCEW(10));  // RT_RCDATA
        HGLOBAL g = r ? LoadResource(nullptr, r) : nullptr;
        const void* p = g ? LockResource(g) : nullptr;
        const DWORD size = r ? SizeofResource(nullptr, r) : 0;
        const bool there = GetFileAttributesW(bat.c_str()) != INVALID_FILE_ATTRIBUTES;
        HANDLE f = p && size ? CreateFileW(bat.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                           FILE_ATTRIBUTE_NORMAL, nullptr)
                             : INVALID_HANDLE_VALUE;
        if (f == INVALID_HANDLE_VALUE && !there)
            return "no " + name + " in the game's folder, and it could not be written there";
        if (f != INVALID_HANDLE_VALUE) {  // else the one there (read-only, say) runs
            DWORD wrote = 0;
            const bool ok = WriteFile(f, p, size, &wrote, nullptr) && wrote == size;
            CloseHandle(f);
            if (!ok) {
                DeleteFileW(bat.c_str());
                return name + " could not be written into the game's folder";
            }
        }
    }
    // In a console of its own, from the game's folder (update.bat checks for uaRO.exe there).
    const HINSTANCE h = ShellExecuteW(nullptr, L"open", bat.c_str(), nullptr, gameDir.c_str(), SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(h) <= 32)
        return name + " could not be started (error " + std::to_string(reinterpret_cast<INT_PTR>(h)) + ")";
    return "";
}

}  // namespace

std::string startUpdate(const std::wstring& gameDir) { return startBat(gameDir, L"update.bat", L"UPDATE_BAT"); }

std::string startGetModels(const std::wstring& gameDir) {
    return startBat(gameDir, L"get-models.bat", L"GET_MODELS_BAT");
}

}  // namespace uagfx::app
