#include "win/sound_hook.hpp"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>

#include "core/client_libs.hpp"
#include "core/config.hpp"
#include "core/sound_mix.hpp"
#include "win/game_camera.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/sound_engine.hpp"

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
using EnumFn = int(WINAPI*)(void* next, void* provider, char** name);
using AllocFn = void*(WINAPI*)(void* driver);
EnumFn g_enum3D = nullptr;
AllocFn g_allocate = nullptr;
std::string g_providers;  // 0.91.1: the 3D providers Miles offered, for the log
// 0.92: left and right, a busy sample stopped for a new sound, the music's pieces crossfaded
using PanFn = void(WINAPI*)(void* sample, int pan);
PanFn g_pan = nullptr;  // not imported by the game: mss32.dll's own export
HandleFn g_end = nullptr, g_startStream = nullptr;
using Play2DFn = char(__cdecl*)(void* file, float* x, float* y, float* z, int farthest, int nearest, uint32_t extra);
constexpr uint32_t kPlay2D = 0x421AF0;                               // a sound at a place (FUN_00421af0)
const uint32_t kPlay2DCalls[] = {0x421D6B, 0x439729, 0x439BF8};       // its three calls, each `add esp, 1Ch` after
constexpr uint32_t kPlay3D = 0x421C20;                               // 1.1: the 3D one (FUN_00421c20)
const uint32_t kPlay3DCalls[] = {0x439700, 0x439BCD};
constexpr uint32_t kModeKind = 0x74B5D0, kModeNow = 0x74B57C;         // the mode manager's: 1 = the game itself
const uint8_t kGetGameMode[] = {0x83, 0x79, 0x58, 0x01, 0x75, 0x04, 0x8B, 0x41, 0x04, 0xC3};  // 0x4F6AD0
const uint8_t kViewMatrix[] = {0x8D, 0x90, 0xC4, 0x00, 0x00, 0x00};  // 0x564A58: lea edx, [view+0C4h] (the ray's)
int g_pendingPan = -1;
unsigned g_stolen = 0;

// The game's voice counts (FUN_004214f0: 48, 32 or 16 by setup.exe) - its tables hold 51 2D and 48 3D handles.
constexpr uint32_t kVoices2D = 0x70E930, kVoices3D = 0x70EC08;
const uint8_t kVoicesCode[] = {0xB8, 0x30, 0x00, 0x00, 0x00, 0xA3, 0x30, 0xE9, 0x70, 0x00};  // 0x421520
constexpr uint32_t kVoicesAt = 0x421520;

// 0.91.1: setup.exe's 3D sound (SOUNDMODE 2) plays the effects through a 3D provider alone - the game looks for
// "Miles Fast 2D Positional Audio" and allocates no 2D samples; without that provider the effects were silent, the
// music alone heard. The 2D samples are allocated then, as SOUNDMODE 1 has them (FUN_00421af0 plays through them).
constexpr uint32_t kSoundMode = 0x7731A8, kDriver = 0x70E748, kSamples2D = 0x70E750, kProvider3D = 0x70EB34;
const uint8_t kModeCode[] = {0xA1, 0xA8, 0x31, 0x77, 0x00};  // 0x421559: mov eax, [SOUNDMODE]
constexpr uint32_t kModeAt = 0x421559;

SoundSettings g_set;
bool g_read = false, g_logged = false, g_front = true;
unsigned g_askedRate = 0, g_openedRate = 0;
int g_voicesWere = 0;

struct Tracked {
    void* handle = nullptr;
    int wanted = 127;   // what the game set
    int applied = -1;   // what Miles has now
    uint8_t kind = 0;   // 0 a sample, 1 a 3D sample, 2 a stream
    ULONGLONG fadeFrom = 0;  // 0.92: a stream fading in (or out) since then; 0 none
    bool out = false;        // the game closed it: fading out, then closed
};
Tracked g_tracked[160];
int g_count = 0;
ULONGLONG g_lastTick = 0;

void readSettings() {
    if (g_read) return;
    g_read = true;
    std::string text;
    if (readTextFile(ourFile(layout::File::Ini), text)) g_set = parseConfig(text).sound;
    audio::configure(g_set.engine, g_set.reverb, g_set.background, g_set.limiter, g_set.headphones);  // 1.1, 1.2
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
double g_duck = 1.0;  // 1.3: the music's share now (the fight's ducking)
int g_crowd = 0;      // the sounds playing at the last count (Miles' and the engine's)

void apply(const Tracked* starting) {
    readSettings();
    int crowd = 0;
    for (int i = 0; i < g_count; ++i)
        if (g_tracked[i].kind != 2 && (&g_tracked[i] == starting || playing(g_tracked[i]))) ++crowd;
    g_crowd = crowd + audio::playing();
    const double back = g_front ? 1.0 : double(g_set.background) / 100.0;
    const double gain = (g_set.limiter ? sound::crowdGain(crowd) : 1.0) * back;
    const ULONGLONG now = GetTickCount64();
    for (int i = 0; i < g_count; ++i) {
        Tracked& t = g_tracked[i];
        double fade = 1.0;
        const double since = double(now - t.fadeFrom);
        if (t.fadeFrom) fade = t.out ? sound::fadeOut(since) : sound::fadeIn(since);
        const int v = sound::scaled(t.wanted, (t.kind == 2 ? back * g_duck : gain) * fade);
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
    if (g_pendingPan >= 0 && g_pan) g_pan(s, g_pendingPan);  // 0.92: the sound being played at its place
    g_start(s);
}

// 0.92: the screen's right in the world (the camera's matrix the mouse's ray is made with: its first row).
bool screenRight(float& rx, float& rz) {
    if (*reinterpret_cast<const int*>(uintptr_t(kModeKind)) != 1) return false;
    const uint32_t mode = *reinterpret_cast<const uint32_t*>(uintptr_t(kModeNow));
    const uint32_t view = mode ? *reinterpret_cast<const uint32_t*>(uintptr_t(mode + 0xC8)) : 0;
    if (!view) return false;
    const auto* m = reinterpret_cast<const float*>(uintptr_t(view + 0xC4));
    rx = m[0];
    rz = m[2];
    return std::isfinite(rx) && std::isfinite(rz);
}

// All the game's samples busy: the quietest stopped, so that the new sound has one.
void stealFor() {
    auto* table = reinterpret_cast<void**>(uintptr_t(kSamples2D));
    const int count = std::min(*reinterpret_cast<const int*>(uintptr_t(kVoices2D)), 51);
    int volumes[51];
    for (int i = 0; i < count; ++i) {
        volumes[i] = -1;
        if (!table[i]) continue;
        if (g_status(table[i]) == 2) return;  // one is free
        volumes[i] = 127;
        for (int k = 0; k < g_count; ++k)
            if (g_tracked[k].handle == table[i] && g_tracked[k].kind == 0) volumes[i] = g_tracked[k].applied;
    }
    const int victim = sound::quietest(volumes, count);
    if (victim < 0) return;
    g_end(table[victim]);
    ++g_stolen;
}

float factorOf(uint32_t bits) {
    float f = 1.0f;
    std::memcpy(&f, &bits, sizeof f);
    return f;
}

char __cdecl play2D(void* file, float* x, float* y, float* z, int farthest, int nearest, uint32_t extra) {
    readSettings();
    // 1.1: the driver's own engine first - around the player, the place's echo
    if (g_set.engine && audio::play(file, *x, *y, *z, farthest, nearest, factorOf(extra))) return 1;
    const double distance = std::sqrt(double(*x) * *x + double(*y) * *y + double(*z) * *z);
    if (g_set.steal && g_end && g_status && distance < double(farthest)) stealFor();
    float rx = 0, rz = 0;
    if (g_set.stereo > 0 && g_pan && screenRight(rx, rz)) {
        const int pan = sound::panOf(*x, *z, rx, rz, g_set.stereo);
        g_pendingPan = g_set.stereoFlip ? 128 - std::max(pan, 1) : pan;
    }
    const char played = reinterpret_cast<Play2DFn>(uintptr_t(kPlay2D))(file, x, y, z, farthest, nearest, extra);
    g_pendingPan = -1;
    return played;
}

// 1.1: likewise the game's 3D sound (setup.exe's 3D: its provider places it) - the engine's when it is on.
char __cdecl play3D(void* file, float* x, float* y, float* z, int farthest, int nearest, uint32_t extra) {
    readSettings();
    if (g_set.engine && audio::play(file, *x, *y, *z, farthest, nearest, factorOf(extra))) return 1;
    return reinterpret_cast<Play2DFn>(uintptr_t(kPlay3D))(file, x, y, z, farthest, nearest, extra);
}

// 0.92: a piece of music fades in; closed by the game, it fades out first (the next one fading in meanwhile).
void WINAPI startStream(void* s) {
    readSettings();
    if (g_set.crossfade)
        if (Tracked* t = track(s, 2)) {
            t->fadeFrom = GetTickCount64();
            t->out = false;
            t->applied = -1;
            apply(nullptr);
        }
    g_startStream(s);
}

// The streams fading out closed for good (all of them when Miles is about to close).
void closeFaded(bool all) {
    const ULONGLONG now = GetTickCount64();
    for (int i = 0; i < g_count;) {
        Tracked& t = g_tracked[i];
        if (t.kind == 2 && t.out && (all || double(now - t.fadeFrom) >= sound::kFadeMs)) {
            void* handle = t.handle;
            t = g_tracked[--g_count];
            g_closeStream(handle);
            continue;
        }
        if (t.kind == 2 && !t.out && t.fadeFrom && double(now - t.fadeFrom) >= sound::kFadeMs) t.fadeFrom = 0;
        ++i;
    }
}

bool fading() {
    for (int i = 0; i < g_count; ++i)
        if (g_tracked[i].fadeFrom) return true;
    return false;
}
void WINAPI start3D(void* s) {
    apply(track(s, 1));
    g_start3D(s);
}

int WINAPI enum3D(void* next, void* provider, char** name) {
    const int more = g_enum3D(next, provider, name);
    if (more && name && *name && g_providers.size() < 400)
        g_providers += (g_providers.empty() ? "" : ", ") + std::string(*name);
    return more;
}

// The game's sound set up (its first frame): 3D sound without its provider gets the 2D samples.
std::string fixMissing3D() {
    if (std::memcmp(reinterpret_cast<const void*>(uintptr_t(kModeAt)), kModeCode, sizeof kModeCode) != 0) return "";
    if (*reinterpret_cast<const int*>(uintptr_t(kSoundMode)) != 2) return "";
    if (*reinterpret_cast<void* const*>(uintptr_t(kProvider3D)))
        return "; 3D sound through its provider (offered: " + (g_providers.empty() ? "none" : g_providers) + ")";
    void* driver = *reinterpret_cast<void* const*>(uintptr_t(kDriver));
    const int count = std::min(*reinterpret_cast<const int*>(uintptr_t(kVoices2D)), 48);
    int made = 0;
    auto* table = reinterpret_cast<void**>(uintptr_t(kSamples2D));
    for (int i = 0; driver && g_allocate && i < count; ++i)
        if (!table[i] && (table[i] = g_allocate(driver)) != nullptr) ++made;
    return "; 3D sound without \"Miles Fast 2D Positional Audio\" (offered: " +
           (g_providers.empty() ? "none" : g_providers) + ") - the effects through " + std::to_string(made) +
           " 2D samples";
}

void WINAPI closeStream(void* s) {
    readSettings();
    if (g_set.crossfade)
        for (int i = 0; i < g_count; ++i)
            if (g_tracked[i].handle == s && g_tracked[i].kind == 2 && !g_tracked[i].out) {
                g_tracked[i].out = true;  // 0.92: closed when faded out
                g_tracked[i].fadeFrom = GetTickCount64();
                g_tracked[i].applied = -1;
                return;
            }
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
    closeFaded(true);
    g_count = 0;
    g_closeDriver(d);
}
void WINAPI close3D(void* p) {
    closeFaded(true);
    g_count = 0;
    g_close3D(p);
}
void WINAPI shutdownMiles() {
    closeFaded(true);
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

// The three calls of the game's sound at a place to play2D - each checked to be `call 0x421AF0; add esp, 1Ch`.
// Calls to `target` at `sites` to `hook`, each checked to be `call target; add esp, 1Ch` first (all or none).
template <size_t N>
void redirect(const uint32_t (&sites)[N], uint32_t target, const void* hook) {
    for (uint32_t at : sites) {
        auto* code = reinterpret_cast<uint8_t*>(uintptr_t(at));
        int32_t rel = 0;
        std::memcpy(&rel, code + 1, 4);
        if (code[0] != 0xE8 || uint32_t(int32_t(at + 5) + rel) != target || code[5] != 0x83 || code[6] != 0xC4 ||
            code[7] != 0x1C)
            return;
    }
    for (uint32_t at : sites) {
        auto* code = reinterpret_cast<uint8_t*>(uintptr_t(at));
        DWORD was = 0;
        if (!VirtualProtect(code + 1, 4, PAGE_EXECUTE_READWRITE, &was)) return;
        const int32_t to = int32_t(reinterpret_cast<uintptr_t>(hook) - (uintptr_t(at) + 5));
        std::memcpy(code + 1, &to, 4);
        DWORD unused = 0;
        VirtualProtect(code + 1, 4, was, &unused);
        FlushInstructionCache(GetCurrentProcess(), code, 5);
    }
}

void installPlay2D() {
    const bool known = std::memcmp(reinterpret_cast<const void*>(uintptr_t(0x4F6AD0)), kGetGameMode,
                                   sizeof kGetGameMode) == 0 &&
                       std::memcmp(reinterpret_cast<const void*>(uintptr_t(0x564A58)), kViewMatrix,
                                   sizeof kViewMatrix) == 0;
    if (!known) return;
    redirect(kPlay2DCalls, kPlay2D, reinterpret_cast<const void*>(&play2D));
    redirect(kPlay3DCalls, kPlay3D, reinterpret_cast<const void*>(&play3D));  // 1.1
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
    readImport("_AIL_allocate_sample_handle@4", g_allocate);  // 0.91.1
    hookImport("_AIL_enumerate_3D_providers@12", &enum3D, g_enum3D);
    hookImport("_AIL_close_stream@4", &closeStream, g_closeStream);
    hookImport("_AIL_release_sample_handle@4", &releaseSample, g_release);
    hookImport("_AIL_release_3D_sample_handle@4", &release3D, g_release3D);
    hookImport("_AIL_close_digital_driver@4", &closeDriver, g_closeDriver);
    hookImport("_AIL_close_3D_provider@4", &close3D, g_close3D);
    hookImport("_AIL_shutdown@0", &shutdownMiles, g_shutdown);
    hookImport("_AIL_open_digital_driver@16", &openDriver, g_open);
    // 0.92: left and right, the busy samples, the music's crossfade
    readImport("_AIL_end_sample@4", g_end);
    if (HMODULE mss = GetModuleHandleA("mss32.dll"))
        g_pan = reinterpret_cast<PanFn>(reinterpret_cast<void*>(GetProcAddress(mss, "_AIL_set_sample_pan@8")));
    hookImport("_AIL_start_stream@4", &startStream, g_startStream);
    installPlay2D();
    hookImport("_AIL_set_sample_volume@8", &sampleVolume, g_sampleVolume);
    hookImport("_AIL_set_3D_sample_volume@8", &volume3D, g_volume3D);
    hookImport("_AIL_set_stream_volume@8", &streamVolume, g_streamVolume);
    hookImport("_AIL_start_sample@4", &startSample, g_start);
    hookImport("_AIL_start_3D_sample@4", &start3D, g_start3D);
}

void soundFrame(bool foreground) {
    if (!g_open) return;
    {  // 1.1: the engine's frame - its voices, the background, the place (the map looked at twice a second)
        static std::string map;
        static ULONGLONG looked = 0;
        const ULONGLONG t = GetTickCount64();
        if (t - looked > 500) {
            looked = t;
            map = readGameMap();
        }
        audio::frame(foreground, map);
    }
    if (!g_logged && g_askedRate) {
        g_logged = true;
        const std::string voices =
            g_voicesWere ? ", 48 sounds at once (the game's " + std::to_string(g_voicesWere) + ")" : "";
        const std::string threeD = fixMissing3D();
        std::string bytes;  // 3.0.1-beta.7: our mss32.dll (Miles over XAudio2, core/client_libs.hpp) or RAD's own
        const HMODULE mss = GetModuleHandleA("mss32.dll");
        const bool ours = mss && readTextFile(modulePath(mss), bytes) && client_libs::ours(bytes);
        LOG_INFO("sound: %s opened at %u Hz (the game asked %u)%s; limiter %s, %d%% in the background%s",
                 ours ? "our mss32.dll (Miles over XAudio2 2.9)" : "Miles", g_openedRate, g_askedRate, voices.c_str(),
                 g_set.limiter ? "on" : "off", g_set.background, threeD.c_str());
    }
    const ULONGLONG now = GetTickCount64();
    {  // 1.3: the music makes way while many sounds play, every frame
        static ULONGLONG before = now;
        const double seconds = double(now - before) / 1000.0;
        before = now;
        const double was = g_duck;
        g_duck = sound::duckStep(g_duck, g_set.duck ? sound::duckTarget(g_crowd) : 1.0, seconds);
        if (std::fabs(g_duck - was) > 0.01) {
            g_front = foreground;
            apply(nullptr);
        }
    }
    if (fading()) {  // 0.92: the music's crossfade: every frame
        g_front = foreground;
        closeFaded(false);
        apply(nullptr);
        return;
    }
    if (foreground == g_front && now - g_lastTick < 100) return;
    g_front = foreground;
    g_lastTick = now;
    apply(nullptr);  // the crowd thinned out, the game came to the front or went behind
}

}  // namespace uagfx::win
