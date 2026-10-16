#include "win/sound_hook.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>

#include "core/config.hpp"
#include "core/sound_mix.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"

namespace uagfx::win {
namespace {

// Miles 6's own (stdcall): the digital driver, the samples (2D and 3D: volume 0-127, status 4 = playing), streams.
using OpenFn = void*(WINAPI*)(unsigned rate, int bits, int channels, unsigned flags);
using VolumeFn = void(WINAPI*)(void* handle, int volume);
using HandleFn = void(WINAPI*)(void* handle);
using StatusFn = unsigned(WINAPI*)(void* handle);
using VoidFn = void(WINAPI*)();
constexpr unsigned kPlaying = 4;

OpenFn g_open = nullptr;
VolumeFn g_sampleVolume = nullptr, g_volume3D = nullptr, g_streamVolume = nullptr;
HandleFn g_start = nullptr, g_start3D = nullptr;
StatusFn g_status = nullptr, g_status3D = nullptr;
HandleFn g_closeStream = nullptr, g_release = nullptr, g_release3D = nullptr, g_closeDriver = nullptr,
         g_close3D = nullptr;
VoidFn g_shutdown = nullptr;

// The game's voice counts (FUN_004214f0: 48, 32 or 16 by setup.exe) - its tables hold 51 2D and 48 3D handles.
constexpr uint32_t kVoices2D = 0x70E930, kVoices3D = 0x70EC08;
const uint8_t kVoicesCode[] = {0xB8, 0x30, 0x00, 0x00, 0x00, 0xA3, 0x30, 0xE9, 0x70, 0x00};  // 0x421520
constexpr uint32_t kVoicesAt = 0x421520;

SoundSettings g_set;
bool g_read = false, g_logged = false, g_front = true;
unsigned g_askedRate = 0, g_openedRate = 0;
int g_voicesWere = 0;

struct Tracked {
    void* handle = nullptr;
    int wanted = 127;   // what the game set
    int applied = -1;   // what Miles has now
    uint8_t kind = 0;   // 0 a sample, 1 a 3D sample, 2 a stream
};
Tracked g_tracked[160];
int g_count = 0;
ULONGLONG g_lastTick = 0;

void readSettings() {
    if (g_read) return;
    g_read = true;
    std::string text;
    if (readTextFile(gameDir() + "\\uaro-graphics.ini", text)) g_set = parseConfig(text).sound;
}

Tracked* track(void* handle, uint8_t kind) {
    for (int i = 0; i < g_count; ++i)
        if (g_tracked[i].handle == handle && g_tracked[i].kind == kind) return &g_tracked[i];
    if (g_count == int(sizeof g_tracked / sizeof g_tracked[0])) return nullptr;
    Tracked& t = g_tracked[g_count++];
    t.handle = handle;
    t.kind = kind;
    return &t;
}

// A handle Miles lets go of is not touched again (a closed stream's memory is freed).
void forget(void* handle, uint8_t kind) {
    for (int i = 0; i < g_count; ++i)
        if (g_tracked[i].handle == handle && g_tracked[i].kind == kind) {
            g_tracked[i] = g_tracked[--g_count];
            return;
        }
}

bool playing(const Tracked& t) {
    if (t.kind == 0) return g_status && g_status(t.handle) == kPlaying;
    if (t.kind == 1) return g_status3D && g_status3D(t.handle) == kPlaying;
    return false;
}

// Every volume anew: the samples playing (and the one about to start) by the crowd, all by the background.
void apply(const Tracked* starting) {
    readSettings();
    int crowd = 0;
    for (int i = 0; i < g_count; ++i)
        if (g_tracked[i].kind != 2 && (&g_tracked[i] == starting || playing(g_tracked[i]))) ++crowd;
    const double back = g_front ? 1.0 : double(g_set.background) / 100.0;
    const double gain = (g_set.limiter ? sound::crowdGain(crowd) : 1.0) * back;
    for (int i = 0; i < g_count; ++i) {
        Tracked& t = g_tracked[i];
        const int v = sound::scaled(t.wanted, t.kind == 2 ? back : gain);
        if (v == t.applied) continue;
        t.applied = v;
        const VolumeFn set = t.kind == 0 ? g_sampleVolume : t.kind == 1 ? g_volume3D : g_streamVolume;
        if (set) set(t.handle, v);
    }
}

void* WINAPI openDriver(unsigned rate, int bits, int channels, unsigned flags) {
    readSettings();
    g_askedRate = rate;
    const unsigned want = sound::rateFor(g_set.rate, rate);
    if (g_set.voices && std::memcmp(reinterpret_cast<const void*>(uintptr_t(kVoicesAt)), kVoicesCode,
                                    sizeof kVoicesCode) == 0) {
        g_voicesWere = *reinterpret_cast<int*>(uintptr_t(kVoices2D));
        *reinterpret_cast<int*>(uintptr_t(kVoices2D)) = 48;  // the handles are allocated after the driver opens
        *reinterpret_cast<int*>(uintptr_t(kVoices3D)) = 48;
    }
    void* driver = g_open(want, want != rate && bits < 16 ? 16 : bits, channels, flags);
    if (!driver && want != rate) {  // not at that rate: as the game asked
        driver = g_open(rate, bits, channels, flags);
        g_openedRate = driver ? rate : 0;
    } else {
        g_openedRate = driver ? want : 0;
    }
    return driver;
}

void WINAPI sampleVolume(void* s, int volume) {
    if (Tracked* t = track(s, 0)) {
        t->wanted = volume;
        t->applied = -1;
        apply(nullptr);
    } else {
        g_sampleVolume(s, volume);
    }
}
void WINAPI volume3D(void* s, int volume) {
    if (Tracked* t = track(s, 1)) {
        t->wanted = volume;
        t->applied = -1;
        apply(nullptr);
    } else {
        g_volume3D(s, volume);
    }
}
void WINAPI streamVolume(void* s, int volume) {
    if (Tracked* t = track(s, 2)) {
        t->wanted = volume;
        t->applied = -1;
        apply(nullptr);
    } else {
        g_streamVolume(s, volume);
    }
}
void WINAPI startSample(void* s) {
    apply(track(s, 0));
    g_start(s);
}
void WINAPI start3D(void* s) {
    apply(track(s, 1));
    g_start3D(s);
}

void WINAPI closeStream(void* s) {
    forget(s, 2);
    g_closeStream(s);
}
void WINAPI releaseSample(void* s) {
    forget(s, 0);
    g_release(s);
}
void WINAPI release3D(void* s) {
    forget(s, 1);
    g_release3D(s);
}
// The driver or the 3D provider closed, Miles shut down: every handle of them gone.
void WINAPI closeDriver(void* d) {
    g_count = 0;
    g_closeDriver(d);
}
void WINAPI close3D(void* p) {
    g_count = 0;
    g_close3D(p);
}
void WINAPI shutdownMiles() {
    g_count = 0;
    g_shutdown();
}

// uaRO.exe's import slot of `name` from mss32.dll; null when it has none.
IMAGE_THUNK_DATA* slotOf(const char* name) {
    auto* base = reinterpret_cast<uint8_t*>(GetModuleHandleA(nullptr));
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!dir.VirtualAddress) return nullptr;
    for (auto* d = reinterpret_cast<const IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); d->Name; ++d) {
        if (lstrcmpiA(reinterpret_cast<const char*>(base + d->Name), "mss32.dll") != 0 || !d->OriginalFirstThunk)
            continue;
        auto* names = reinterpret_cast<const IMAGE_THUNK_DATA*>(base + d->OriginalFirstThunk);
        auto* slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + d->FirstThunk);
        for (; names->u1.AddressOfData; ++names, ++slots) {
            if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal)) continue;
            const auto* by = reinterpret_cast<const IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData);
            if (std::strcmp(reinterpret_cast<const char*>(by->Name), name) == 0) return slots;
        }
    }
    return nullptr;
}

// The import `name` to `hook`, the old one kept in `old`.
template <typename Fn>
bool hookImport(const char* name, Fn hook, Fn& old) {
    IMAGE_THUNK_DATA* slot = slotOf(name);
    DWORD was = 0;
    if (!slot || !VirtualProtect(&slot->u1.Function, sizeof(void*), PAGE_READWRITE, &was)) return false;
    old = reinterpret_cast<Fn>(slot->u1.Function);
    slot->u1.Function = reinterpret_cast<uintptr_t>(hook);
    DWORD unused = 0;
    VirtualProtect(&slot->u1.Function, sizeof(void*), was, &unused);
    return true;
}

template <typename Fn>
void readImport(const char* name, Fn& out) {
    if (IMAGE_THUNK_DATA* slot = slotOf(name)) out = reinterpret_cast<Fn>(slot->u1.Function);
}

}  // namespace

void installSoundHooks() {
    static bool done = false;
    if (done) return;
    done = true;
    // Every one there before any is replaced: the volumes are not taken over without the handles' ends.
    for (const char* name : {"_AIL_open_digital_driver@16", "_AIL_set_sample_volume@8", "_AIL_set_3D_sample_volume@8",
                             "_AIL_set_stream_volume@8", "_AIL_start_sample@4", "_AIL_start_3D_sample@4",
                             "_AIL_sample_status@4", "_AIL_3D_sample_status@4", "_AIL_close_stream@4",
                             "_AIL_release_sample_handle@4", "_AIL_release_3D_sample_handle@4",
                             "_AIL_close_digital_driver@4", "_AIL_close_3D_provider@4", "_AIL_shutdown@0"})
        if (!slotOf(name)) return;  // no Miles, or not this one: the game's sound as it is
    readImport("_AIL_sample_status@4", g_status);  // the limiter counts by them
    readImport("_AIL_3D_sample_status@4", g_status3D);
    hookImport("_AIL_close_stream@4", &closeStream, g_closeStream);
    hookImport("_AIL_release_sample_handle@4", &releaseSample, g_release);
    hookImport("_AIL_release_3D_sample_handle@4", &release3D, g_release3D);
    hookImport("_AIL_close_digital_driver@4", &closeDriver, g_closeDriver);
    hookImport("_AIL_close_3D_provider@4", &close3D, g_close3D);
    hookImport("_AIL_shutdown@0", &shutdownMiles, g_shutdown);
    hookImport("_AIL_open_digital_driver@16", &openDriver, g_open);
    hookImport("_AIL_set_sample_volume@8", &sampleVolume, g_sampleVolume);
    hookImport("_AIL_set_3D_sample_volume@8", &volume3D, g_volume3D);
    hookImport("_AIL_set_stream_volume@8", &streamVolume, g_streamVolume);
    hookImport("_AIL_start_sample@4", &startSample, g_start);
    hookImport("_AIL_start_3D_sample@4", &start3D, g_start3D);
}

void soundFrame(bool foreground) {
    if (!g_open) return;
    if (!g_logged && g_askedRate) {
        g_logged = true;
        const std::string voices =
            g_voicesWere ? ", 48 sounds at once (the game's " + std::to_string(g_voicesWere) + ")" : "";
        LOG_INFO("sound: Miles opened at %u Hz (the game asked %u)%s; limiter %s, %d%% in the background", g_openedRate,
                 g_askedRate, voices.c_str(), g_set.limiter ? "on" : "off", g_set.background);
    }
    const ULONGLONG now = GetTickCount64();
    if (foreground == g_front && now - g_lastTick < 100) return;
    g_front = foreground;
    g_lastTick = now;
    apply(nullptr);  // the crowd thinned out, the game came to the front or went behind
}

}  // namespace uagfx::win
