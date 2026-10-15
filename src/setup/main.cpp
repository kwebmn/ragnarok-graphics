// uaro-graphics-setup.exe (0.60): uaRO Graphics in one file for a player - the release's zip appended to this
// program (tools/package.sh: the zip, its size in eight bytes, "UAROSETP"). It finds the uaRO client folder (its own,
// else the player chooses it), unpacks the zip there, runs install.bat /quiet and opens the settings app, where
// "Set up automatically" and "Download the pack" are the next presses.
#include <windows.h>

#include <objbase.h>
#include <shlobj.h>
#include <shellapi.h>

#include <cstdio>
#include <string>
#include <vector>

#include "core/zip.hpp"

namespace {

enum Lang { En, Uk, Ru };

Lang language() {
    const LANGID id = GetUserDefaultUILanguage();
    const WORD primary = PRIMARYLANGID(id);
    return primary == LANG_UKRAINIAN ? Uk : primary == LANG_RUSSIAN ? Ru : En;
}

const wchar_t* text(int which) {
    static const wchar_t* t[][3] = {
        {L"uaRO Graphics setup", L"Встановлення uaRO Graphics", L"Установка uaRO Graphics"},
        {L"Choose the uaRO client folder (the one with uaRO.exe):", L"Виберіть теку клієнта uaRO (де лежить uaRO.exe):",
         L"Выберите папку клиента uaRO (где лежит uaRO.exe):"},
        {L"There is no uaRO.exe in that folder. Run the setup again and choose the uaRO client folder.",
         L"У цій теці немає uaRO.exe. Запустіть встановлення ще раз і виберіть теку клієнта uaRO.",
         L"В этой папке нет uaRO.exe. Запустите установку ещё раз и выберите папку клиента uaRO."},
        {L"This setup is damaged (no archive inside it). Download it again.",
         L"Це встановлення пошкоджене (у ньому немає архіву). Завантажте його ще раз.",
         L"Эта установка повреждена (в ней нет архива). Скачайте её ещё раз."},
        {L"Cannot write into the uaRO folder: close the game and the settings app; a client under Program Files needs "
         L"the setup run as administrator.",
         L"Не вдається записати в теку uaRO: закрийте гру й утиліту; клієнт у Program Files потребує запуску "
         L"встановлення від імені адміністратора.",
         L"Не удаётся записать в папку uaRO: закройте игру и утилиту; клиенту в Program Files нужен запуск установки "
         L"от имени администратора."},
        {L"install.bat did not finish: run install.bat in the uaRO folder by hand to see why.",
         L"install.bat не завершився: запустіть install.bat у теці uaRO вручну, щоб побачити причину.",
         L"install.bat не завершился: запустите install.bat в папке uaRO вручную, чтобы увидеть причину."},
        {L"uaRO Graphics is installed. The settings app opens now: press \"Set up automatically\" on its first page, "
         L"then \"Download the pack\" on the Texture pack page, and start the game.",
         L"uaRO Graphics встановлено. Зараз відкриється утиліта: натисніть «Налаштувати автоматично» на першій "
         L"сторінці, потім «Завантажити пакет» на сторінці «Пакет текстур» і запускайте гру.",
         L"uaRO Graphics установлен. Сейчас откроется утилита: нажмите «Настроить автоматически» на первой странице, "
         L"потом «Скачать пакет» на странице «Пакет текстур» и запускайте игру."},
    };
    return t[which][language()];
}

int fail(int which) {
    MessageBoxW(nullptr, text(which), text(0), MB_ICONERROR | MB_OK);
    return 1;
}

std::wstring folderOf(const std::wstring& path) {
    const size_t slash = path.find_last_of(L"\\/");
    return slash == std::wstring::npos ? L"." : path.substr(0, slash);
}

bool exists(const std::wstring& path) { return GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES; }

std::string ansi(const std::wstring& w) {
    const int n = WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string out(size_t(n > 0 ? n - 1 : 0), '\0');
    if (n > 1) WideCharToMultiByte(CP_ACP, 0, w.c_str(), -1, &out[0], n, nullptr, nullptr);
    return out;
}

// The appended zip into a file of its own (zip::Index reads files).
bool payload(const std::wstring& exe, const std::wstring& to) {
    HANDLE f = CreateFileW(exe.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    GetFileSizeEx(f, &size);
    bool ok = false;
    char tail[16];
    DWORD got = 0;
    LARGE_INTEGER at{};
    at.QuadPart = size.QuadPart - 16;
    if (size.QuadPart > 16 && SetFilePointerEx(f, at, nullptr, FILE_BEGIN) && ReadFile(f, tail, 16, &got, nullptr) &&
        got == 16 && std::string(tail + 8, 8) == "UAROSETP") {
        unsigned long long zipSize = 0;
        for (int i = 7; i >= 0; --i) zipSize = zipSize << 8 | static_cast<unsigned char>(tail[i]);
        at.QuadPart = size.QuadPart - 16 - LONGLONG(zipSize);
        HANDLE out = CreateFileW(to.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (zipSize && at.QuadPart > 0 && out != INVALID_HANDLE_VALUE && SetFilePointerEx(f, at, nullptr, FILE_BEGIN)) {
            std::vector<char> buf(1 << 20);
            unsigned long long left = zipSize;
            ok = true;
            while (left && ok) {
                const DWORD want = DWORD(left < buf.size() ? left : buf.size());
                DWORD r = 0, w = 0;
                ok = ReadFile(f, buf.data(), want, &r, nullptr) && r == want &&
                     WriteFile(out, buf.data(), r, &w, nullptr) && w == r;
                left -= r;
            }
        }
        if (out != INVALID_HANDLE_VALUE) CloseHandle(out);
    }
    CloseHandle(f);
    return ok;
}

std::wstring chooseFolder() {
    BROWSEINFOW bi{};
    bi.lpszTitle = text(1);
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE id = SHBrowseForFolderW(&bi);
    if (!id) return L"";
    wchar_t path[MAX_PATH] = {};
    SHGetPathFromIDListW(id, path);
    CoTaskMemFree(id);
    return path;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    wchar_t self[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, self, MAX_PATH);
    std::wstring dir = folderOf(self);
    if (!exists(dir + L"\\uaRO.exe")) {
        dir = chooseFolder();
        if (dir.empty()) return 0;  // cancelled
        if (!exists(dir + L"\\uaRO.exe")) return fail(2);
    }
    wchar_t temp[MAX_PATH] = {};
    GetTempPathW(MAX_PATH, temp);
    const std::wstring zipPath = std::wstring(temp) + L"uaro-graphics-setup.zip";
    if (!payload(self, zipPath)) return fail(3);
    uagfx::zip::Index zip;
    std::string error;
    // The names as stored (zip::normalName lower-cases them): every one kept, read back by its normal name.
    std::vector<std::string> stored;
    if (!zip.open(ansi(zipPath), [&](const std::string&) { return true; }, error)) return fail(3);
    for (const std::string& name : zip.names()) {
        if (name.empty() || name.back() == '\\' || name.back() == '/') continue;
        std::vector<uint8_t> bytes;
        if (!zip.read(name, bytes, error)) return fail(3);
        std::string path = ansi(dir) + "\\" + name;
        for (size_t i = ansi(dir).size() + 1; i < path.size(); ++i)
            if (path[i] == '\\') {
                CreateDirectoryA(path.substr(0, i).c_str(), nullptr);
            }
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return fail(4);
        const bool ok = bytes.empty() || std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        if (std::fclose(f) != 0 || !ok) return fail(4);
    }
    DeleteFileW(zipPath.c_str());
    // install.bat as a player would run it, without its questions.
    std::wstring line = L"cmd.exe /c \"\"" + dir + L"\\install.bat\" /quiet\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, &line[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, dir.c_str(), &si, &pi))
        return fail(5);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (code != 0) return fail(5);
    MessageBoxW(nullptr, text(6), text(0), MB_ICONINFORMATION | MB_OK);
    ShellExecuteW(nullptr, L"open", (dir + L"\\uaro-graphics.exe").c_str(), nullptr, dir.c_str(), SW_SHOWNORMAL);
    CoUninitialize();
    return 0;
}
