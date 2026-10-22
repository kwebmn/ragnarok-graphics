#pragma once
#include <string>
#include <vector>

namespace uagfx::app {

std::wstring exeFolder();  // the folder of uaro-graphics.exe: the client folder
bool readFile(const std::wstring& path, std::string& out);
// Written to "<path>.new" and moved over the old file in one step: a reader never sees half of it (spec §4.5).
bool writeFileAtomic(const std::wstring& path, const std::string& bytes);
bool fileExists(const std::wstring& path);
long long fileSize(const std::wstring& path);  // 2.14: -1 when there is no such file
bool dirExists(const std::wstring& path);
std::vector<std::wstring> listFolder(const std::wstring& dir, bool folders);  // names, sorted
bool makeFolder(const std::wstring& path);   // also when it exists
bool removeTree(const std::wstring& path);
bool deleteFile(const std::wstring& path);  // 0.55
// 2.8: one rename (MoveFileExW without copying: a pack of gigabytes never is); replace: a file at `to` goes
bool moveFile(const std::wstring& from, const std::wstring& to, bool replace);
bool removeEmptyFolder(const std::wstring& path);  // 2.8
bool folderWritable(const std::wstring& dir);
bool underProgramFiles(const std::wstring& dir);
std::wstring timestampName();  // "2026-10-13-192033", for a backup folder
// .reg files are UTF-16 with a BOM; the core reads and writes UTF-8.
std::string regFileToUtf8(const std::string& bytes);
std::string utf8ToRegFile(const std::string& utf8);
// 0 when started; else the Windows error.
unsigned long launch(const std::wstring& file, const std::wstring& folder);
// 2.10: with arguments, and as administrator ("runas": Windows asks) when `elevated`; 0 or the Windows error.
unsigned long launchArgs(const std::wstring& file, const std::wstring& args, const std::wstring& folder,
                         bool elevated);
void openInShell(const std::wstring& path);
// The text of dist/uaro-graphics.ini built into the app (resource DEFAULT_INI).
std::string defaultIni();

}  // namespace uagfx::app
