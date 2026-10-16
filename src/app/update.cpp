#include "app/update.hpp"

#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>

#include "app/text.hpp"
#include "core/update_check.hpp"

namespace uagfx::app {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
UpdateState g_state;

void set(const UpdateState& s) {
    AcquireSRWLockExclusive(&g_lock);
    g_state = s;
    ReleaseSRWLockExclusive(&g_lock);
}

// GET https://host/path with one header more; the body, or "" and why.
std::string get(const wchar_t* host, const std::wstring& path, const wchar_t* header, std::string& error) {
    std::string body;
    HINTERNET session = WinHttpOpen(L"uaRO Graphics Settings", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                     WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET connect = session ? WinHttpConnect(session, host, INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER,
                                                     WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE)
                                : nullptr;
    DWORD status = 0, size = sizeof status;
    const bool sent = request &&
                      WinHttpSendRequest(request, header ? header : WINHTTP_NO_ADDITIONAL_HEADERS,
                                         header ? DWORD(-1L) : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
                      WinHttpReceiveResponse(request, nullptr) &&
                      WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                          WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (!sent) {
        error = "no answer from " + utf8(host) + " (error " + std::to_string(GetLastError()) + ")";
    } else if (status != 200) {
        error = utf8(host) + " answered " + std::to_string(status);
    } else {
        DWORD n = 0;
        while (WinHttpQueryDataAvailable(request, &n) && n && body.size() < (1u << 20)) {
            std::string part(n, '\0');
            DWORD read = 0;
            if (!WinHttpReadData(request, &part[0], n, &read) || !read) break;
            body.append(part, 0, read);
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    return body;
}

DWORD WINAPI check(void*) {
    UpdateState s;
    std::string error;
    std::string commit = get(L"api.github.com", wide(update::kCommitPath), L"Accept: application/vnd.github.sha",
                             error);
    while (!commit.empty() && (commit.back() == '\n' || commit.back() == '\r')) commit.pop_back();
    if (!update::isCommit(commit)) {
        s.kind = UpdateState::Failed;
        s.error = error.empty() ? "GitHub gave no commit" : error;
        set(s);
        return 0;
    }
    s.latest = update::versionOf(get(L"raw.githubusercontent.com", wide(update::versionPath(commit)), nullptr, error));
    s.kind = s.latest.empty() ? UpdateState::Failed : UpdateState::Done;
    if (s.latest.empty()) s.error = error.empty() ? "no version in the newest commit" : error;
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

std::string startUpdate(const std::wstring& gameDir) {
    const std::wstring bat = gameDir + L"\\update.bat";
    if (GetFileAttributesW(bat.c_str()) == INVALID_FILE_ATTRIBUTES) {
        // 1.3.1: a client copied without it - the one this app carries goes there first
        HRSRC r = FindResourceW(nullptr, L"UPDATE_BAT", MAKEINTRESOURCEW(10));  // RT_RCDATA
        HGLOBAL g = r ? LoadResource(nullptr, r) : nullptr;
        const void* p = g ? LockResource(g) : nullptr;
        const DWORD size = r ? SizeofResource(nullptr, r) : 0;
        HANDLE f = p && size ? CreateFileW(bat.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                                           nullptr)
                             : INVALID_HANDLE_VALUE;
        if (f == INVALID_HANDLE_VALUE) return "no update.bat in the game's folder, and it could not be written there";
        DWORD wrote = 0;
        const bool ok = WriteFile(f, p, size, &wrote, nullptr) && wrote == size;
        CloseHandle(f);
        if (!ok) {
            DeleteFileW(bat.c_str());
            return "update.bat could not be written into the game's folder";
        }
    }
    // In a console of its own, from the game's folder (it checks for uaRO.exe there).
    const HINSTANCE h = ShellExecuteW(nullptr, L"open", bat.c_str(), nullptr, gameDir.c_str(), SW_SHOWNORMAL);
    if (reinterpret_cast<INT_PTR>(h) <= 32)
        return "update.bat could not be started (error " + std::to_string(reinterpret_cast<INT_PTR>(h)) + ")";
    return "";
}

}  // namespace uagfx::app
