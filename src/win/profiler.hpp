#pragma once
#include <windows.h>

#include <string>
#include <vector>

namespace uagfx::win {

// 0.87, [debug] profile: samples the game's thread about a thousand times a second from a thread of its own - the
// game's thread stopped for the moment of reading where it is (and a little of its stack, for the uaRO.exe place
// that called into another module), then let go before anything else - and writes the tally (core/sample_profile.hpp)
// into the log every ten seconds. Off: the thread sleeps.
// 3.0.2-beta.12: a map's loading by itself (Context's watch, core/map_load.hpp) - sampled from open() to close(), its
// tally into the log then; each sample also says Windows' function it is in ("Sleep", "ReadFile") and the game's
// step it is under (the world, its files, a frame shown).
class Profiler {
public:
    static Profiler& get();
    void run(DWORD gameThread, bool on);  // each present: on/off as the settings say, started the first time
    // 3.0.2-beta.12: a map's loading - its tally's lines under `prefix` ("map prontera"), the first with `head`'s
    // words ("loaded in 3.69 s"), then `more`; drop(): none
    void open(DWORD gameThread);
    void close(const std::string& prefix, const std::string& head, const std::vector<std::string>& more);
    void drop();
    void stop();  // the game closing
    // 3.0.11-beta.94 (R-3S33MK «чтото лагает игра»: the game's thread stood still 15 s and 5 s, the log silent on
    // where): each present - the sampling thread started for the stall watch (once); the game's message loop's beats
    // (win/stall_watch) looked at four times a second: still for a second - sampled until it beats again, then «the
    // game's thread: stood still N s» with the tally (where it waited: Windows' function and who called it)
    void watch(DWORD gameThread, HWND window);

private:
    enum : LONG { kShut, kOpen, kReport, kDrop };  // window_
    bool start(DWORD gameThread);                   // the sampling thread, once
    static DWORD WINAPI loop(void* self);
    void sampleLoop();
    HANDLE thread_ = nullptr;
    HANDLE game_ = nullptr;
    HANDLE wake_ = nullptr;  // 3.0.2-beta.12: something to sample (in place of looking every 200 ms)
    bool failed_ = false;    // the game's thread cannot be opened: said once
    volatile LONG on_ = 0;
    volatile LONG window_ = kShut;
    volatile LONG quit_ = 0;
    volatile LONG watching_ = 0;   // 3.0.11-beta.94: the stall watch on
    bool watchAsked_ = false;      // the game's thread's: watch() did its work
    HWND volatile gameWindow_ = nullptr;
    CRITICAL_SECTION lock_;  // close()'s words, for the sampling thread
    std::string prefix_, head_;
    std::vector<std::string> more_;
};

}  // namespace uagfx::win
