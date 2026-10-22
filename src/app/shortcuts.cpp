#include "app/shortcuts.hpp"

#include <windows.h>

#include <objbase.h>
#include <shlobj.h>

namespace uagfx::app {
namespace {

// COM for the calls of one function: this thread may not have it yet (the update's worker).
struct Com {
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ~Com() {
        if (SUCCEEDED(hr)) CoUninitialize();
    }
};

}  // namespace

std::wstring desktopFolder() {
    wchar_t path[MAX_PATH] = {};
    return SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_DESKTOPDIRECTORY, nullptr, SHGFP_TYPE_CURRENT, path)) ? path : L"";
}

std::vector<Shortcut> desktopShortcuts() {
    std::vector<Shortcut> out;
    const std::wstring desk = desktopFolder();
    if (desk.empty()) return out;
    Com com;
    WIN32_FIND_DATAW f{};
    HANDLE find = FindFirstFileW((desk + L"\\*.lnk").c_str(), &f);
    if (find == INVALID_HANDLE_VALUE) return out;
    do {
        IShellLinkW* link = nullptr;
        if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                    reinterpret_cast<void**>(&link))))
            continue;
        IPersistFile* file = nullptr;
        Shortcut s;
        s.path = desk + L"\\" + f.cFileName;
        wchar_t target[MAX_PATH] = {};
        if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file)))) {
            if (SUCCEEDED(file->Load(s.path.c_str(), STGM_READ)) &&
                SUCCEEDED(link->GetPath(target, MAX_PATH, nullptr, SLGP_RAWPATH)))
                s.target = target;
            file->Release();
        }
        link->Release();
        if (!s.target.empty()) out.push_back(s);
    } while (FindNextFileW(find, &f));
    FindClose(find);
    return out;
}

bool makeShortcut(const std::wstring& path, const std::wstring& target, const std::wstring& folder,
                  const std::wstring& icon) {
    Com com;
    IShellLinkW* link = nullptr;
    if (FAILED(CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW,
                                reinterpret_cast<void**>(&link))))
        return false;
    link->SetPath(target.c_str());
    link->SetWorkingDirectory(folder.c_str());
    link->SetIconLocation(icon.c_str(), 0);
    link->SetDescription(L"uaRO");
    IPersistFile* file = nullptr;
    bool ok = false;
    if (SUCCEEDED(link->QueryInterface(IID_IPersistFile, reinterpret_cast<void**>(&file)))) {
        ok = SUCCEEDED(file->Save(path.c_str(), TRUE));
        file->Release();
    }
    link->Release();
    return ok;
}

}  // namespace uagfx::app
