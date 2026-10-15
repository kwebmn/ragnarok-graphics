#pragma once
#include <windows.h>

namespace uagfx::win {

// 0.87, [debug] profile: samples the game's thread about a thousand times a second from a thread of its own - the
// game's thread stopped for the moment of reading where it is (and a little of its stack, for the uaRO.exe place
// that called into another module), then let go before anything else - and writes the tally (core/sample_profile.hpp)
// into the log every ten seconds. Off: the thread sleeps.
class Profiler {
public:
    static Profiler& get();
    void run(DWORD gameThread, bool on);  // each present: on/off as the settings say, started the first time
    void stop();                          // the game closing

private:
    static DWORD WINAPI loop(void* self);
    void sampleLoop();
    HANDLE thread_ = nullptr;
    HANDLE game_ = nullptr;
    volatile LONG on_ = 0;
    volatile LONG quit_ = 0;
};

}  // namespace uagfx::win
