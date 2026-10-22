#!/usr/bin/env python3
"""Checks the cross-built uaro-graphics.exe, the settings app (stage 2 spec §3.1).

- It is a 32-bit Windows GUI program.
- It imports only the functions on the list below: system DLLs and functions Windows 7 has. The list is of what is
  allowed, not of what is not (0.6.0 review M7): a new import fails until someone checks that Windows 7 has it and
  adds it; a newer function is found at run time (GetProcAddress) instead.
- Its manifest asks for no administrator rights (asInvoker) and per-monitor DPI.
"""
import struct
import sys

# Every function the settings app imports, each one Windows 7 has.
ALLOWED = {
    "advapi32.dll": {
        "RegQueryValueExW",  # 0.51.1: the video memory from the display driver's key
        "GetTokenInformation", "OpenProcessToken", "RegCloseKey", "RegCreateKeyExW", "RegDeleteTreeW",
        "RegDeleteValueW", "RegEnumKeyExW", "RegEnumValueW", "RegOpenKeyExW", "RegSetValueExW",
    },
    "comctl32.dll": {
        "InitCommonControlsEx",
    },
    "comdlg32.dll": {
        "GetOpenFileNameW", "GetSaveFileNameW",
    },
    "gdi32.dll": {
        "CreateFontIndirectW", "DeleteObject", "GetDeviceCaps", "GetTextExtentPoint32W", "SelectObject",
        "SetBkMode", "SetTextColor",
        # 1.6.1: the preview's zoom (all of them since Windows 2000)
        "CreateCompatibleBitmap", "CreateCompatibleDC", "DeleteDC", "GetObjectW", "SetBrushOrgEx", "SetStretchBltMode",
        "StretchBlt",
        "BitBlt", "CreateSolidBrush",  # 1.6.2: the preview's window
    },
    "kernel32.dll": {
        "GetFullPathNameW",  # 2.5.19: the palettes' source and result told apart (Windows 2000 and later)
        # 0.49: the update check's thread and its lock
        "AcquireSRWLockExclusive", "AcquireSRWLockShared", "CreateThread", "ReleaseSRWLockExclusive",
        "ReleaseSRWLockShared",
        "CloseHandle", "CompareFileTime", "CreateDirectoryW", "CreateFileW", "CreateSemaphoreW", "CreateToolhelp32Snapshot",
        "DeleteCriticalSection", "DeleteFileW", "EnterCriticalSection", "FindClose", "FindFirstFileW",
        "FindNextFileW", "FindResourceW", "FlushFileBuffers", "FormatMessageA", "FreeLibrary", "GetCurrentProcess",
        "GetCurrentThreadId", "GetEnvironmentVariableW", "GetFileAttributesExW", "GetFileAttributesW",
        "GetFileSizeEx", "GetLastError",
        "GetLocalTime", "GetModuleFileNameW", "GetModuleHandleA", "GetModuleHandleW", "GetProcAddress",
        "GetStartupInfoW", "GetSystemDirectoryA", "GetSystemDirectoryW", "GetUserDefaultUILanguage", "GlobalAlloc",
        "GlobalFree", "GlobalLock", "GlobalUnlock", "InitializeCriticalSection", "IsDBCSLeadByteEx",
        "IsWow64Process", "LeaveCriticalSection", "LoadLibraryA", "LoadLibraryW", "LoadResource", "LocalFree",
        "LockResource", "MoveFileExW", "MulDiv", "MultiByteToWideChar", "OpenMutexW", "OpenProcess",
        "Process32FirstW",
        "Process32NextW", "QueryFullProcessImageNameW", "ReadFile", "ReleaseSemaphore", "RemoveDirectoryW",
        "SetLastError", "SetUnhandledExceptionFilter", "SizeofResource", "Sleep", "TlsAlloc", "TlsFree",
        "TlsGetValue", "TlsSetValue", "VirtualProtect", "VirtualQuery", "WaitForSingleObject",
        "WideCharToMultiByte", "WriteFile", "lstrcmpiW", "lstrcpyW",
        # 0.43, the texture pack page: its builder started and watched (Windows 2000 / Vista)
        "CreateProcessW", "GetTickCount64",
        # 0.51: the watcher of a game for the reports (--watch)
        "SetPriorityClass", "GetSystemTimeAsFileTime",  # 0.52: a crash dump of the last week
        "CreateMutexA", "ReleaseMutex",  # 0.53: one writer at a time (the statistics' lock now)
        # 2.5.28: the Texture pack page's Compact - the pack's locks, the copy put in its place, the disk's room
        # (Windows 2000 and later)
        "CreateMutexW", "DeleteFileA", "MoveFileExA", "GetDiskFreeSpaceExW",
        # 2.5.15: the computer in the report - its cores (XP SP3), memory and power (Windows 2000)
        "GetLogicalProcessorInformation", "GlobalMemoryStatusEx", "GetSystemPowerStatus",
        # 2.10: the update's downloads - a number against caches, a .part continued at its end (Windows 2000); the
        # removal's copy of the app in %TEMP%, the process the update waits for (Windows NT)
        "GetTickCount", "SetFilePointerEx", "CopyFileW", "GetTempPathW", "GetCurrentProcessId",
        "SetEnvironmentVariableW",  # 2.11: UARO_LAUNCHER for the game the launcher starts (Windows NT)
    },
    "msvcrt.dll": {
        "__mb_cur_max", "__p__commode", "__p__fmode", "__p__wcmdln", "__set_app_type", "__setusermatherr",
        "__wgetmainargs", "__winitenv", "_amsg_exit", "_cexit", "_close", "_errno", "_filelengthi64", "_fileno",
        "_initterm", "_iob", "_onexit", "_read", "_wfopen", "abort", "atoi", "calloc", "clock", "exit", "fclose",
        "fflush",
        "fgetpos", "fopen", "fprintf", "fputc", "fputs", "fputwc", "fread", "free", "fsetpos", "fwprintf",
        "fwrite", "isalnum", "isupper", "iswctype", "isxdigit", "localeconv", "malloc", "memchr", "memcmp", "memcpy",
        "memmove", "memset", "realloc",
        "setlocale", "signal", "strchr", "strcmp", "strcoll", "strerror", "strftime", "strlen", "strncmp", "strtol",
        "strtoul", "strxfrm", "tolower", "toupper", "towlower", "towupper", "vfprintf", "wcscoll", "wcsftime",
        "wcslen", "wcsxfrm",
        # 0.43: the texture pack's reader (core/texture_pack.cpp), the builder's process name, libwebp
        "_chsize", "_wcsicmp", "islower", "isspace",
        "wcsstr", "wcstoul", "time", "_wcstoui64",  # 0.51: --watch, the reports sent
    },
    "ole32.dll": {
        "CoInitializeEx", "CoUninitialize",
        "CoCreateInstance", "CreateStreamOnHGlobal", "GetHGlobalFromStream",  # 1.8.1: screenshots for a report
        "CoTaskMemFree",  # 2.5.19: the palettes' folder
        "CoCreateGuid",  # 2.13: the computer's key for «Звернення»
    },
    "winhttp.dll": {  # 0.49: the update check
        "WinHttpCloseHandle", "WinHttpConnect", "WinHttpOpen", "WinHttpOpenRequest", "WinHttpQueryDataAvailable",
        "WinHttpQueryHeaders", "WinHttpReadData", "WinHttpReceiveResponse", "WinHttpSendRequest",
        "WinHttpSetTimeouts", "WinHttpCrackUrl",  # 0.51: the reports to the developers
    },
    "shell32.dll": {
        "SHGetFolderPathW", "ShellExecuteExW", "ShellExecuteW",
        "SHBrowseForFolderW", "SHGetPathFromIDListW",  # 2.5.19: the palettes' folder (Windows 2000 and later)
    },
    "user32.dll": {
        "IsWindowEnabled", "IsWindowVisible",  # 2.10: the progress window's Cancel, the status bar's Update
        # 1.6.2: the preview's window
        "BeginPaint", "EndPaint", "FillRect", "InvalidateRect", "ReleaseCapture", "ScreenToClient", "SetCapture",
        "SetCursor",  # 0.51: waiting for uaro.kiev.ua
        "GetAncestor",  # 0.49: the update check
        "AdjustWindowRectEx", "CloseClipboard", "CreateWindowExW", "DefWindowProcW", "DestroyWindow",
        "DispatchMessageW", "DrawTextW", "EmptyClipboard", "EnableWindow", "EnumDisplayMonitors",
        "EnumDisplaySettingsExW", "FindWindowW", "GetClientRect", "GetDC", "GetDlgCtrlID", "GetDlgItem", "GetFocus",
        "GetMessageW", "GetMonitorInfoW", "GetNextDlgTabItem", "GetSysColor", "GetSysColorBrush", "GetWindowLongW",
        "GetWindowRect", "GetWindowTextW", "IsChild", "IsDialogMessageW", "KillTimer", "LoadCursorW", "LoadIconW", "LoadImageW",
        "MapWindowPoints", "MessageBoxW", "MoveWindow", "OpenClipboard", "PostMessageW", "PostQuitMessage",
        "RegisterClassExW", "ReleaseDC", "SendMessageW", "SetClipboardData", "SetDlgItemTextW", "SetFocus",
        "SetForegroundWindow", "SetTimer", "SetWindowLongW", "SetWindowPos", "SetWindowTextW", "ShowWindow",
        "SystemParametersInfoW", "TranslateMessage", "wsprintfW",
        # 0.22.1, the page view that scrolls (all of them in Windows since 95 / NT 3.1)
        "GetClassNameW", "GetScrollInfo", "GetSystemMetrics", "ScrollWindowEx", "SetScrollInfo", "SetScrollPos",
        "WindowFromPoint",
        # 0.43, the texture pack page's lines
        "GetWindowTextLengthW",
        # 2.6, the window that keeps its size and place (Windows 2000 and later)
        "GetWindowPlacement", "SetWindowPlacement", "MonitorFromRect", "MonitorFromWindow",
        "DrawFocusRect",  # 2.6: the list of views, drawn by the app (Windows 95 and later)
        "GetKeyState",  # 2.6: Ctrl+F, the search
        "IsWindow",  # 2.6: "What's new"'s window (Windows 95 and later)
    },
}
# Newer than Windows 7: never on the list above (a check of the list itself).
NOT_ON_WINDOWS_7 = {"GetDpiForWindow", "GetDpiForSystem", "SystemParametersInfoForDpi", "AdjustWindowRectExForDpi",
                    "SetThreadDpiAwarenessContext", "SetProcessDpiAwarenessContext", "GetDpiForMonitor",
                    "EnableNonClientDpiScaling", "GetSystemMetricsForDpi", "D3D12CreateDevice", "CreateDXGIFactory1",
                    "CreateDXGIFactory2"}


def fail(msg):
    print("FAIL: " + msg)
    sys.exit(1)


def main(path):
    try:
        data = open(path, "rb").read()
    except OSError:
        fail(path + " not found")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        fail("not a PE file")
    machine, nsec = struct.unpack_from("<HH", data, pe + 4)
    if machine != 0x14C:
        fail("not a 32-bit (i386) program")
    optsize = struct.unpack_from("<H", data, pe + 20)[0]
    opt = pe + 24
    if struct.unpack_from("<H", data, opt)[0] != 0x10B:
        fail("not PE32")
    if struct.unpack_from("<H", data, opt + 68)[0] != 2:
        fail("not a GUI program (subsystem %d)" % struct.unpack_from("<H", data, opt + 68)[0])
    imp_rva = struct.unpack_from("<I", data, opt + 104)[0]
    sections = [struct.unpack_from("<8sIIII", data, opt + optsize + i * 40) for i in range(nsec)]

    def off(rva):
        for _, vsize, va, rsize, raw in sections:
            if va <= rva < va + max(vsize, rsize):
                return rva - va + raw
        fail("rva %#x is outside the sections" % rva)

    def cstr(rva):
        o = off(rva)
        return data[o:data.index(b"\0", o)].decode()

    listed_late = sorted(set().union(*ALLOWED.values()) & NOT_ON_WINDOWS_7)
    if listed_late:
        fail("the list holds functions Windows 7 does not have: " + ", ".join(listed_late))
    dlls, imports = [], []
    o = off(imp_rva)
    while struct.unpack_from("<I", data, o + 12)[0]:
        lookup, _, _, name, first = struct.unpack_from("<IIIII", data, o)
        dll = cstr(name).lower()
        dlls.append(dll)
        t = off(lookup or first)
        while True:
            entry = struct.unpack_from("<I", data, t)[0]
            if entry == 0:
                break
            if not entry & 0x80000000:
                imports.append((dll, cstr(entry + 2)))
            t += 4
        o += 20
    bad = [d for d in dlls if d not in ALLOWED]
    if bad:
        fail("unexpected imports: " + ", ".join(bad))
    unknown = sorted("%s!%s" % (d, f) for d, f in imports if f not in ALLOWED[d])
    if unknown:
        fail("imports functions not on the list: " + ", ".join(unknown) +
             " - check that Windows 7 has them, then add them to ALLOWED")
    for needed in (b"asInvoker", b"PerMonitorV2", b"Microsoft.Windows.Common-Controls"):
        if needed not in data:
            fail("the manifest has no " + needed.decode())
    print("OK: %s: GUI, %d functions from %s, asInvoker, per-monitor DPI" % (path, len(imports), " ".join(dlls)))


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "build-win/uaro-graphics.exe")
