#include "win/laptop_frames.hpp"

#include <windows.h>

#include <string>

#include "core/laptop_set.hpp"
#include "core/layout.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win::laptopframes {
namespace {

constexpr ULONGLONG kPowerEveryMs = 3000, kWriteEveryMs = 60000;
constexpr double kLongestFrameS = 0.25;  // a map's loading or a stall: not a frame to count

struct State {
    bool read = false, mains = false, changed = false, setOn = false;
    ULONGLONG powerAt = 0, writtenAt = 0, setAt = 0;
    int writes = 0;
    LARGE_INTEGER last{};
    laptopset::Meter meter;
};

State& state() {
    static State s;
    return s;
}

std::string path() { return gameDir() + "\\" + layout::kLaptopFrames; }

}  // namespace

void frame(bool laptop, bool countable, const Config& config, int fpsLimit) {
    if (!laptop) return;
    State& s = state();
    static LARGE_INTEGER freq{};
    if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const double seconds = s.last.QuadPart ? double(now.QuadPart - s.last.QuadPart) / double(freq.QuadPart) : 0.0;
    s.last = now;
    if (!s.read) {
        s.read = true;
        std::string text;
        laptopset::Record r;
        if (readTextFile(path(), text) && laptopset::parse(text, r)) s.meter.set(r);
    }
    const ULONGLONG tick = GetTickCount64();
    if (!s.powerAt || tick - s.powerAt >= kPowerEveryMs) {
        s.powerAt = tick;
        SYSTEM_POWER_STATUS p{};
        s.mains = GetSystemPowerStatus(&p) && p.ACLineStatus == 1;
    }
    if (!s.setAt || tick - s.setAt >= 1000) {
        s.setAt = tick;
        s.setOn = laptopset::on(config);
    }
    if (countable && s.mains && (fpsLimit == 0 || fpsLimit >= 60) && seconds > 0.0 && seconds <= kLongestFrameS) {
        s.meter.frame(seconds, s.setOn);
        s.changed = true;
    }
    if (s.changed && (!s.writtenAt || tick - s.writtenAt >= kWriteEveryMs)) {
        s.writtenAt = tick;
        s.changed = false;
        const laptopset::Record& r = s.meter.record();
        if (writeFileAtomic(path(), laptopset::format(r)) && s.writes++ % 10 == 0)  // the log every 10 minutes
            LOG_INFO("laptop: the frames on the maps plugged in - without «Для ноутбука» %.1f fps over %.1f min, with "
                     "it %.1f over %.1f",
                     r.other.fps, r.other.minutes, r.laptop.fps, r.laptop.minutes);
    }
}

}  // namespace uagfx::win::laptopframes
