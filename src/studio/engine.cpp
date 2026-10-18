#include "studio/engine.hpp"

#include <cstdio>
#include <utility>
#include <vector>

#include "app/text.hpp"
#include "core/studio_link.hpp"
#include "core/studio_state.hpp"
#include "studio/messages.hpp"
#include "studio/places.hpp"

namespace uagfx::studio {
namespace {

int64_t unixOf(const FILETIME& t) {
    return int64_t((uint64_t(t.dwHighDateTime) << 32 | t.dwLowDateTime) / 10000000ull) - 11644473600ll;
}

std::wstring wideAscii(const std::string& s) {
    std::wstring w;
    for (char c : s) w += wchar_t(static_cast<unsigned char>(c));
    return w;
}

}  // namespace

bool installedEngine(const std::wstring& gameDir, std::string& versionLine, uint64_t& size, int64_t& time) {
    WIN32_FILE_ATTRIBUTE_DATA d{};
    if (!GetFileAttributesExW((gameDir + L"\\uaro-graphics\\uaro-render64.exe").c_str(), GetFileExInfoStandard, &d))
        return false;
    size = uint64_t(d.nFileSizeHigh) << 32 | d.nFileSizeLow;
    time = unixOf(d.ftLastWriteTime);
    versionLine.clear();
    if (std::FILE* f = _wfopen((gameDir + L"\\uaro-graphics\\version.txt").c_str(), L"rb")) {
        char buf[128] = {};
        if (std::fgets(buf, sizeof buf, f)) versionLine = buf;
        std::fclose(f);
    }
    while (!versionLine.empty() && (versionLine.back() == '\n' || versionLine.back() == '\r')) versionLine.pop_back();
    return true;
}

bool Engine::start(const std::wstring& gameDir, std::wstring& error) {
    stop();
    std::string line;
    uint64_t size = 0;
    int64_t time = 0;
    if (!installedEngine(gameDir, line, size, time)) {
        error = L"no uaro-render64.exe in " + gameDir + L"\\uaro-graphics";
        return false;
    }
    const std::string name = engineName(line, size, time);
    const std::wstring root = studioRoot(), exe = root + L"\\" + wideAscii(name);
    makeFolder(root);
    if (GetFileAttributesW(exe.c_str()) == INVALID_FILE_ATTRIBUTES &&
        !CopyFileW((gameDir + L"\\uaro-graphics\\uaro-render64.exe").c_str(), exe.c_str(), TRUE)) {
        error = L"cannot copy the engine to " + exe;
        return false;
    }
    // The copies of the last five driver builds stay (F3 compares two of them); older ones go.
    std::vector<std::pair<std::string, int64_t>> copies;
    WIN32_FIND_DATAW f{};
    HANDLE find = FindFirstFileW((root + L"\\engine-*.exe").c_str(), &f);
    if (find != INVALID_HANDLE_VALUE) {
        do copies.emplace_back(app::utf8(f.cFileName), unixOf(f.ftLastWriteTime));
        while (FindNextFileW(find, &f));
        FindClose(find);
    }
    for (const std::string& old : buildsToDrop(copies, name)) DeleteFileW((root + L"\\" + app::wide(old)).c_str());
    // Its pipe: this studio's, a new name each start.
    static std::atomic<int> starts{0};
    const std::string pipe = "uaro-studio-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(++starts);
    pipe_ = CreateNamedPipeW(app::wide("\\\\.\\pipe\\" + pipe).c_str(), PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                             PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1, 1 << 16, 1 << 16, 0, nullptr);
    if (pipe_ == INVALID_HANDLE_VALUE) {
        error = L"cannot make the engine's pipe";
        return false;
    }
    std::wstring command = L"\"" + exe + L"\" --studio " + app::wide(pipe) + L" --dir \"" + gameDir + L"\"";
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    const DWORD flags = CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS;
    if (!CreateProcessW(exe.c_str(), &command[0], nullptr, nullptr, FALSE, flags, nullptr, root.c_str(), &si, &pi)) {
        error = L"cannot start " + exe;
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
        return false;
    }
    CloseHandle(pi.hThread);
    process_ = pi.hProcess;
    // It connects within ten seconds, or it is no engine.
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    bool connected = ConnectNamedPipe(pipe_, &ov) != 0 || GetLastError() == ERROR_PIPE_CONNECTED;
    if (!connected && GetLastError() == ERROR_IO_PENDING) {
        const HANDLE both[2] = {ov.hEvent, process_};
        connected = WaitForMultipleObjects(2, both, FALSE, 10000) == WAIT_OBJECT_0;
        if (!connected) CancelIoEx(pipe_, &ov);
    }
    CloseHandle(ov.hEvent);
    if (!connected) {
        error = L"the engine did not answer: " + exe;
        stop();
        return false;
    }
    name_ = name;
    gone_ = false;
    reader_ = CreateThread(nullptr, 0, &Engine::readMain, this, 0, nullptr);
    return reader_ != nullptr;
}

void Engine::stop() {
    if (process_ && !gone_) tell("quit", json::Value());
    if (process_) {
        if (WaitForSingleObject(process_, 3000) != WAIT_OBJECT_0) TerminateProcess(process_, 1);
        CloseHandle(process_);
        process_ = nullptr;
    }
    if (pipe_ != INVALID_HANDLE_VALUE) CancelIoEx(pipe_, nullptr);
    if (reader_) {
        WaitForSingleObject(reader_, 5000);
        CloseHandle(reader_);
        reader_ = nullptr;
    }
    AcquireSRWLockExclusive(&write_);
    if (pipe_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipe_);
        pipe_ = INVALID_HANDLE_VALUE;
    }
    ReleaseSRWLockExclusive(&write_);
    endAll("the engine stopped");
    name_.clear();
}

bool Engine::write(const std::string& line) {
    AcquireSRWLockExclusive(&write_);  // stop() closes the pipe under it
    if (pipe_ == INVALID_HANDLE_VALUE || gone_) {
        ReleaseSRWLockExclusive(&write_);
        return false;
    }
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    DWORD n = 0;
    bool ok = WriteFile(pipe_, line.data(), DWORD(line.size()), nullptr, &ov) != 0;
    if (!ok && GetLastError() == ERROR_IO_PENDING) ok = GetOverlappedResult(pipe_, &ov, &n, TRUE) != 0;
    CloseHandle(ov.hEvent);
    ReleaseSRWLockExclusive(&write_);
    return ok;
}

bool Engine::tell(const std::string& op, const json::Value& body) {
    return write(lineOf(Message{next_++, op, body}));
}

bool Engine::call(const std::string& op, const json::Value& body, json::Value& result, std::string& why,
                  DWORD timeoutMs, const Progress& progress) {
    auto w = std::make_shared<Waiting>();
    w->done = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    w->progress = progress;
    const int64_t id = next_++;
    AcquireSRWLockExclusive(&lock_);
    waiting_[id] = w;
    ReleaseSRWLockExclusive(&lock_);
    bool ok = write(lineOf(Message{id, op, body}));
    if (!ok) {
        w->why = "the engine is not running";
    } else if (WaitForSingleObject(w->done, timeoutMs) != WAIT_OBJECT_0) {
        ok = false;
        w->why = "the engine did not answer in time";
    }
    AcquireSRWLockExclusive(&lock_);
    waiting_.erase(id);
    ReleaseSRWLockExclusive(&lock_);
    CloseHandle(w->done);
    if (ok && !w->ok) ok = false;
    if (ok) result = std::move(w->result);
    else why = w->why;
    return ok;
}

void Engine::endAll(const std::string& why) {
    AcquireSRWLockExclusive(&lock_);
    for (auto& w : waiting_) {
        w.second->ok = false;
        w.second->why = why;
        SetEvent(w.second->done);
    }
    ReleaseSRWLockExclusive(&lock_);
}

bool Engine::newerInstalled(const std::wstring& gameDir) const {
    std::string line;
    uint64_t size = 0;
    int64_t time = 0;
    return !name_.empty() && installedEngine(gameDir, line, size, time) && engineName(line, size, time) != name_;
}

DWORD WINAPI Engine::readMain(void* self) {
    static_cast<Engine*>(self)->read();
    return 0;
}

void Engine::read() {
    LineReader lines;
    char buf[4096];
    DWORD n = 0;
    OVERLAPPED ov{};
    ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    for (;;) {
        ResetEvent(ov.hEvent);
        if (!ReadFile(pipe_, buf, sizeof buf, nullptr, &ov) && GetLastError() != ERROR_IO_PENDING) break;
        if (!GetOverlappedResult(pipe_, &ov, &n, TRUE) || !n) break;
        lines.feed(buf, n);
        Message m;
        while (lines.next(m)) {
            if (!m.id) {  // the engine's own line
                if (m.op == "log") PostMessageW(notify_, kEngineLog, 0, LPARAM(new std::string(m.body.asString())));
                continue;
            }
            AcquireSRWLockExclusive(&lock_);
            const auto it = waiting_.find(m.id);
            std::shared_ptr<Waiting> w = it == waiting_.end() ? nullptr : it->second;
            ReleaseSRWLockExclusive(&lock_);
            if (!w) continue;
            if (m.op == "progress") {
                if (w->progress) w->progress(m.body);
                continue;
            }
            w->ok = m.op == "result";
            w->result = m.body;
            if (!w->ok) w->why = m.body["why"].asString().empty() ? "the engine failed" : m.body["why"].asString();
            SetEvent(w->done);
        }
    }
    CloseHandle(ov.hEvent);
    gone_ = true;
    endAll("the engine stopped");
    PostMessageW(notify_, kEngineGone, 0, 0);
}

}  // namespace uagfx::studio
