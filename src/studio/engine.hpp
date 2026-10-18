#pragma once
#include <windows.h>

#include <atomic>
#include <functional>
#include <map>
#include <memory>
#include <string>

#include "core/json.hpp"

namespace uagfx::studio {

// 2.3 (G3): the studio's engine - a copy of the installed uaro-render64.exe in the studio's folder
// (engine-<version>-<build>.exe, core/studio_state.hpp) run as --studio, its pipe the studio's (core/studio_link.hpp).
// A call waits for its reply on the caller's thread (a job's); the engine's own lines go to the window (kEngineLog).
class Engine {
public:
    explicit Engine(HWND notify) : notify_(notify) {}
    ~Engine() { stop(); }
    Engine(const Engine&) = delete;
    Engine& operator=(const Engine&) = delete;

    // The newest installed engine copied (when it is not there yet) and started; the copies of the last five
    // driver builds stay.
    bool start(const std::wstring& gameDir, std::wstring& error);
    void stop();  // "quit", then ended after 3 s; every waiting call gets an error
    bool running() const { return process_ != nullptr && !gone_; }
    using Progress = std::function<void(const json::Value& body)>;
    // A request and its reply: true with the result's body; false with why (an error reply, the engine gone, or no
    // reply in `timeoutMs`).
    bool call(const std::string& op, const json::Value& body, json::Value& result, std::string& why,
              DWORD timeoutMs = INFINITE, const Progress& progress = nullptr);
    bool tell(const std::string& op, const json::Value& body);  // a request no one waits for ("stop")
    // The installed engine is another build than the one running (uaro-graphics\version.txt or the exe changed).
    bool newerInstalled(const std::wstring& gameDir) const;
    std::string name() const { return name_; }  // the running copy's file name, "" none

private:
    struct Waiting {
        HANDLE done = nullptr;
        bool ok = false;
        json::Value result;
        std::string why;
        Progress progress;
    };
    static DWORD WINAPI readMain(void* self);
    void read();
    bool write(const std::string& line);
    void endAll(const std::string& why);  // every waiting call fails

    HWND notify_;
    HANDLE pipe_ = INVALID_HANDLE_VALUE, process_ = nullptr, reader_ = nullptr;
    std::string name_;
    std::atomic<bool> gone_{false};
    std::atomic<int64_t> next_{1};
    SRWLOCK lock_ = SRWLOCK_INIT;   // waiting_
    SRWLOCK write_ = SRWLOCK_INIT;  // one line at a time into the pipe
    std::map<int64_t, std::shared_ptr<Waiting>> waiting_;
};

// The installed engine's build: its version line, its size and time; false when it is not there.
bool installedEngine(const std::wstring& gameDir, std::string& versionLine, uint64_t& size, int64_t& time);

}  // namespace uagfx::studio
