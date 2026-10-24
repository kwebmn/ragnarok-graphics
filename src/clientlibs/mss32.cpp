// 3.0.1-beta.7 (the user: our own libraries in place of the client's old ones): mss32.dll - RAD's Miles Sound System
// 6 as uaRO.exe uses it (0x4214F0-0x421E23, 0x439900), over XAudio2 2.9 (Windows 10 and later: installed only where
// it is). The effects through 2D samples or the 3D ones of "Miles Fast 2D Positional Audio" (placed left and right
// around the listener, quieter with the distance), the music read from BGM\*.mp3 by a thread of its own, the sound
// files' own (core/miles: PCM, IMA and Microsoft ADPCM, mp3). The 46 functions the game takes and
// _AIL_set_sample_pan@8 the driver looks up (win/sound_hook.cpp), by their decorated names (stdcall, mss32.def).
// Every handle may come NULL: the game sets a stream's volume and loop without one (0x421A75) and asks a missing
// stream's volume (0x4219AD).
// 3.0.4-beta.1: on the output device the driver passes ([sound] output, _UAGFX_set_output@4): the chosen one, Windows'
// default while it is not connected; a device lost - everything on a new engine (core/audio_output.hpp).
#include <windows.h>
#include <xaudio2.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <new>
#include <utility>
#include <vector>

#include "core/audio_output.hpp"
#include "core/miles.hpp"
#include "win/audio_devices.hpp"
#include "win/audio_master.hpp"

namespace miles = uagfx::miles;
namespace output = uagfx::audio_output;
namespace devices = uagfx::win::audio_devices;

extern "C" {
// Told from RAD's mss32.dll by these bytes (core/client_libs.hpp)
__attribute__((used)) const char kUaroMark[] = "uaRO Graphics client library: mss32.dll (Miles over XAudio2 2.9)";
}

namespace {

using CreateFn = HRESULT(WINAPI*)(IXAudio2** out, UINT32 flags, UINT32 processor);
using CreateVersionFn = HRESULT(WINAPI*)(IXAudio2** out, UINT32 flags, UINT32 processor, DWORD ntddi);

constexpr unsigned kDone = 2, kPlaying = 4;  // SMP_DONE (a new handle's too: the game looks for it), SMP_PLAYING
constexpr uint32_t kProvider = 1;            // the one 3D provider
const char kProviderName[] = "Miles Fast 2D Positional Audio";  // the game takes it by this name (0x6B7B6C)
constexpr int kBuffers = 4;                  // a stream's buffers queued, kFrames frames each (~0.37 s at 44.1 kHz)
constexpr size_t kFrames = 4096;
constexpr ULONGLONG kLetGoMs = 500;          // a stopped voice's sound kept that long: XAudio2 lets go within a pass
constexpr size_t kMostIdle = 64;             // voices kept stopped for a sound of their format

// Miles' AILSOUNDINFO (32-bit)
struct SoundInfo {
    int32_t format;
    const void* data;
    uint32_t dataLen, rate;
    int32_t bits, channels;
    uint32_t samples, blockSize;
    const void* initial;
};
static_assert(sizeof(SoundInfo) == 36, "Miles 6's AILSOUNDINFO");

// What a handle is: its first field
enum class Kind : uint32_t { None = 0, Sample = 0x55414D31, Sample3D, Listener, Stream };

struct Driver;

struct Sample {
    Kind kind;
    Driver* driver;
    IXAudio2SourceVoice* voice = nullptr;
    uint32_t key = 0;  // the voice's format
    miles::Pcm pcm;    // its sound
    bool started = false;
    int volume = 127, pan = 64;
    float pos[3] = {0, 0, 0}, farthest = 1e9f, nearest = 1.0f;  // a 3D one's place, from the listener's
    Sample(Kind k, Driver* d) : kind(k), driver(d) {}
};

struct Listener {
    Kind kind = Kind::Listener;
    float pos[3] = {0, 0, 0}, front[3] = {0, 0, 1}, up[3] = {0, 1, 0};
};

struct Stream {
    Kind kind = Kind::Stream;
    Driver* driver;
    IXAudio2SourceVoice* voice = nullptr;
    miles::Music music;
    std::vector<int16_t> buffers[kBuffers];
    int next = 0;
    int volume = 127;
    bool started = false, paused = false;
    explicit Stream(Driver* d) : driver(d) {}
};

struct Idle {
    IXAudio2SourceVoice* voice;
    uint32_t key;
    ULONGLONG since;
};

struct Driver {
    HMODULE dll = nullptr;
    IXAudio2* x = nullptr;
    IXAudio2MasteringVoice* master = nullptr;
    CRITICAL_SECTION lock;  // the streams: the game's calls and the thread feeding them
    std::vector<Stream*> streams;
    std::vector<Sample*> samples;  // the game's thread alone
    std::vector<Idle> idle;
    HANDLE quit = nullptr, thread = nullptr;
    // 3.0.4-beta.1: the device it plays on ("" Windows' default), the chosen one not connected, its openings
    std::string opened;
    bool away = false;
    unsigned openings = 0;
    bool watching = false;  // the feed's thread alone: Windows' notices of devices looked at, `seen` of them
    unsigned seen = 0;
};

Driver* g_driver = nullptr;  // the game opens one
Listener g_listener;
int g_speakers = 0;  // AIL_3D_2_SPEAKER
int g_prefs[64] = {};
char g_redist[MAX_PATH] = ".\\";
std::vector<std::pair<ULONGLONG, std::vector<uint8_t>>> g_letGo;  // sounds a voice stopped this pass may still read
std::string g_wanted;        // 3.0.4-beta.1: [sound] output from the driver ("" Windows' default); the driver's lock
volatile LONG g_reopen = 0;  // a device lost (XAudio2's thread) or the chosen one back (the feed's): a new engine

// XAudio2's word that its device is gone (XAUDIO2_E_DEVICE_INVALIDATED): the engine stopped for good
struct EngineWatch : IXAudio2EngineCallback {
    void STDMETHODCALLTYPE OnProcessingPassStart() override {}
    void STDMETHODCALLTYPE OnProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnCriticalError(HRESULT) override { InterlockedExchange(&g_reopen, 1); }
};
EngineWatch g_engineWatch;

Kind kindOf(const void* h) { return h ? *static_cast<const Kind*>(h) : Kind::None; }
Sample* sampleOf(void* h, Kind k) { return kindOf(h) == k ? static_cast<Sample*>(h) : nullptr; }
Stream* streamOf(void* h) { return kindOf(h) == Kind::Stream ? static_cast<Stream*>(h) : nullptr; }

WAVEFORMATEX formatOf(int channels, int bits, uint32_t rate) {
    WAVEFORMATEX f{};
    f.wFormatTag = WAVE_FORMAT_PCM;
    f.nChannels = WORD(channels);
    f.nSamplesPerSec = rate;
    f.wBitsPerSample = WORD(bits);
    f.nBlockAlign = WORD(channels * bits / 8);
    f.nAvgBytesPerSec = rate * f.nBlockAlign;
    return f;
}

// A sound a voice may still read kept a while; those let go long enough freed
void letGo(std::vector<uint8_t>&& bytes) {
    const ULONGLONG now = GetTickCount64();
    g_letGo.erase(std::remove_if(g_letGo.begin(), g_letGo.end(),
                                 [&](const std::pair<ULONGLONG, std::vector<uint8_t>>& g) {
                                     return now - g.first > kLetGoMs;
                                 }),
                  g_letGo.end());
    if (!bytes.empty()) g_letGo.emplace_back(now, std::move(bytes));
    bytes.clear();
}

// A voice for a sound of format `key`: one set aside long enough ago and empty (one whose Flush came before its Stop
// took effect is emptied now), else a new one
IXAudio2SourceVoice* takeVoice(Driver& d, uint32_t key, const WAVEFORMATEX& f) {
    const ULONGLONG now = GetTickCount64();
    for (size_t i = 0; i < d.idle.size(); ++i) {
        if (d.idle[i].key != key || now - d.idle[i].since <= kLetGoMs) continue;
        XAUDIO2_VOICE_STATE st{};
        d.idle[i].voice->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (st.BuffersQueued) {
            d.idle[i].voice->FlushSourceBuffers();
            continue;
        }
        IXAudio2SourceVoice* v = d.idle[i].voice;
        d.idle.erase(d.idle.begin() + long(i));
        return v;
    }
    IXAudio2SourceVoice* v = nullptr;
    return d.x && SUCCEEDED(d.x->CreateSourceVoice(&v, &f, 0, 1.0f, nullptr, nullptr, nullptr)) ? v : nullptr;
}

// A voice stopped and set aside for a later sound of its format. XAudio2 stops it at its next pass (a sound started
// on it meanwhile would follow the rest of the old one) and destroying it would wait for that pass.
void giveVoice(Driver& d, IXAudio2SourceVoice* v, uint32_t key) {
    v->Stop(0, XAUDIO2_COMMIT_NOW);
    v->FlushSourceBuffers();
    d.idle.push_back({v, key, GetTickCount64()});
    if (d.idle.size() > kMostIdle) {
        d.idle.front().voice->DestroyVoice();
        d.idle.erase(d.idle.begin());
    }
}

// The sample's volume and place: a 2D one by its pan, a 3D one by where it is from the listener
void mix(Sample& s) {
    if (!s.voice) return;
    float gain = miles::gainOf(s.volume);
    int pan = s.pan;
    if (s.kind == Kind::Sample3D) {
        const float rel[3] = {s.pos[0] - g_listener.pos[0], s.pos[1] - g_listener.pos[1],
                              s.pos[2] - g_listener.pos[2]};
        float reach = 1.0f, side = 0.5f;
        miles::place3D(rel, g_listener.front, g_listener.up, s.nearest, s.farthest, reach, side);
        gain *= reach;
        pan = int(std::lround(side * 127.0f));
    }
    float left = 1.0f, right = 1.0f;
    miles::panGains(pan, left, right);
    const float mono[2] = {left, right}, stereo[4] = {left, 0.0f, 0.0f, right};
    s.voice->SetOutputMatrix(nullptr, UINT32(s.pcm.channels), 2, s.pcm.channels == 2 ? stereo : mono,
                             XAUDIO2_COMMIT_NOW);
    s.voice->SetVolume(gain, XAUDIO2_COMMIT_NOW);
}

unsigned statusOf(Sample* s) {
    if (!s || !s->started || !s->voice) return kDone;
    XAUDIO2_VOICE_STATE st{};
    s->voice->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    if (st.BuffersQueued) return kPlaying;
    s->started = false;
    return kDone;
}

// Stopped at once: a voice still playing set aside, the next sound on another one
void stop(Sample& s) {
    if (statusOf(&s) == kPlaying) {
        giveVoice(*s.driver, s.voice, s.key);
        s.voice = nullptr;
    }
    s.started = false;
}

// From its start, on its voice when that is of its format and done (a done voice stays started: it plays the new
// sound at once), else on another
void start(Sample& s) {
    if (!s.driver || s.pcm.bytes.empty()) return;
    stop(s);
    const uint32_t key = uint32_t(s.pcm.channels) << 28 | uint32_t(s.pcm.bits) << 20 | s.pcm.rate;
    if (s.voice && s.key != key) {
        giveVoice(*s.driver, s.voice, s.key);
        s.voice = nullptr;
    }
    if (!s.voice) {
        s.voice = takeVoice(*s.driver, key, formatOf(s.pcm.channels, s.pcm.bits, s.pcm.rate));
        s.key = key;
    }
    if (!s.voice) return;
    mix(s);
    XAUDIO2_BUFFER b{};
    b.Flags = XAUDIO2_END_OF_STREAM;
    b.AudioBytes = UINT32(s.pcm.bytes.size());
    b.pAudioData = s.pcm.bytes.data();
    s.started = SUCCEEDED(s.voice->SubmitSourceBuffer(&b, nullptr)) && SUCCEEDED(s.voice->Start(0, XAUDIO2_COMMIT_NOW));
}

int setFile(Sample* s, const void* image) {
    if (!s) return 0;
    stop(*s);
    letGo(std::move(s->pcm.bytes));
    try {
        const auto* p = static_cast<const uint8_t*>(image);
        return miles::pcmOf(p, miles::imageSize(p), s->pcm) ? 1 : 0;
    } catch (const std::bad_alloc&) {
        s->pcm.bytes.clear();
        return 0;
    }
}

void release(Sample* s) {
    if (!s) return;
    if (Driver* d = s->driver) {
        if (s->voice) giveVoice(*d, s->voice, s->key);
        d->samples.erase(std::remove(d->samples.begin(), d->samples.end(), s), d->samples.end());
    }
    letGo(std::move(s->pcm.bytes));
    delete s;
}

// The 3D ones playing placed again: the listener turned or moved
void placeAll() {
    if (!g_driver) return;
    for (Sample* s : g_driver->samples)
        if (s->kind == Kind::Sample3D && s->started) mix(*s);
}

// Its next buffer of the music (the last one marked so)
void submitNext(Stream& s) {
    std::vector<int16_t>& b = s.buffers[s.next];
    b.resize(kFrames * size_t(s.music.channels()));
    const size_t frames = s.music.fill(b.data(), kFrames);
    XAUDIO2_BUFFER x{};
    x.Flags = s.music.ended() ? XAUDIO2_END_OF_STREAM : 0;
    x.AudioBytes = UINT32(frames * size_t(s.music.channels()) * 2);
    x.pAudioData = reinterpret_cast<const BYTE*>(b.data());
    if (frames && SUCCEEDED(s.voice->SubmitSourceBuffer(&x, nullptr))) s.next = (s.next + 1) % kBuffers;
}

// Under the driver's lock
void topUp(Stream& s) {
    if (!s.voice || !s.started || s.music.ended()) return;
    XAUDIO2_VOICE_STATE st{};
    s.voice->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
    for (UINT32 queued = st.BuffersQueued; queued < UINT32(kBuffers) && !s.music.ended(); ++queued)
        submitNext(s);
}

// A voice of its own for the music, fed from where it was read and started unless paused (under the driver's lock)
bool newVoice(Driver& d, Stream& s) {
    const WAVEFORMATEX f = formatOf(s.music.channels(), 16, s.music.rate());
    s.voice = nullptr;
    if (!d.x || FAILED(d.x->CreateSourceVoice(&s.voice, &f, 0, 1.0f, nullptr, nullptr, nullptr))) {
        s.voice = nullptr;
        return false;
    }
    const float mono[2] = {1.0f, 1.0f}, stereo[4] = {1.0f, 0.0f, 0.0f, 1.0f};  // a mono one on both sides
    const int channels = s.music.channels();
    s.voice->SetOutputMatrix(nullptr, UINT32(channels), 2, channels == 2 ? stereo : mono, XAUDIO2_COMMIT_NOW);
    s.voice->SetVolume(miles::gainOf(s.volume), XAUDIO2_COMMIT_NOW);
    s.next = 0;
    s.started = true;
    try {
        topUp(s);
    } catch (const std::bad_alloc&) {
    }
    if (!s.paused) s.voice->Start(0, XAUDIO2_COMMIT_NOW);
    return true;
}

// 3.0.4-beta.1: while it plays on Windows' default instead of the chosen device (or on none), Windows' notices of
// devices looked at: the chosen one connected again (or any device there now) - a new engine, on the game's thread
bool deviceBack(Driver& d, bool away, bool none, const std::string& wanted) {
    if (!away && !none) {
        d.watching = false;
        return false;
    }
    const unsigned now = devices::changes();  // the first call starts the watching
    if (!d.watching || now == d.seen) {
        d.watching = true;
        d.seen = now;
        return false;
    }
    d.seen = now;
    if (none) return true;
    std::vector<devices::Device> active;
    std::string ignored;
    if (!devices::list(active, ignored)) return false;
    std::vector<std::string> ids;
    for (const devices::Device& a : active) ids.push_back(a.id);
    return !output::pick(wanted, ids).id.empty();
}

DWORD WINAPI feed(void* p) {
    Driver& d = *static_cast<Driver*>(p);
    while (WaitForSingleObject(d.quit, 20) == WAIT_TIMEOUT) {
        EnterCriticalSection(&d.lock);
        try {
            for (Stream* s : d.streams) topUp(*s);
        } catch (const std::bad_alloc&) {
        }
        const bool away = d.away, none = !d.x;
        std::string wanted;
        if (away) wanted = g_wanted;
        LeaveCriticalSection(&d.lock);
        if (deviceBack(d, away, none, wanted)) InterlockedExchange(&g_reopen, 1);
    }
    return 0;
}

// A file of the game's folder whole (the music: a few MB)
bool readWhole(const char* name, std::vector<uint8_t>& out) {
    HANDLE f = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                           FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    bool ok = GetFileSizeEx(f, &size) && size.QuadPart > 0 && size.QuadPart < (LONGLONG(256) << 20);
    if (ok) {
        out.resize(size_t(size.QuadPart));
        DWORD got = 0;
        ok = ReadFile(f, out.data(), DWORD(out.size()), &got, nullptr) && got == out.size();
    }
    CloseHandle(f);
    return ok;
}

// A new sound file image the game frees with AIL_mem_free_lock
int give(const std::vector<uint8_t>& image, void** out, unsigned* size) {
    void* p = std::malloc(image.size());
    if (!p) return 0;
    std::memcpy(p, image.data(), image.size());
    *out = p;
    if (size) *size = unsigned(image.size());
    return 1;
}

// The engine and its mastering voice, stereo as Miles played it (Windows sends it on to the speakers there are) - on
// the chosen device when it is connected, else on Windows' default (3.0.4-beta.1)
bool openEngine(Driver& d, const std::string& wanted) {
    const auto create =
        d.dll ? reinterpret_cast<CreateFn>(reinterpret_cast<void*>(GetProcAddress(d.dll, "XAudio2Create"))) : nullptr;
    const auto createVersion =  // newer Windows 10 has the SDK's own call too
        d.dll ? reinterpret_cast<CreateVersionFn>(
                    reinterpret_cast<void*>(GetProcAddress(d.dll, "XAudio2CreateWithVersionInfo")))
              : nullptr;
    output::Target t;
    if (!wanted.empty()) {  // Windows asked only then (a thread for COM)
        std::vector<devices::Device> active;
        std::string ignored;
        std::vector<std::string> ids{wanted};  // no answer: tried as it is
        if (devices::list(active, ignored)) {
            ids.clear();
            for (const devices::Device& a : active) ids.push_back(a.id);
        }
        t = output::pick(wanted, ids);
    }
    HRESULT hr = createVersion ? createVersion(&d.x, 0, 1, 0x0A000000) : create ? create(&d.x, 0, 1) : E_FAIL;
    if (SUCCEEDED(hr)) d.x->RegisterForCallbacks(&g_engineWatch);
    if (SUCCEEDED(hr)) {
        hr = devices::openMaster(d.x, &d.master, 2, t.id);
        if (FAILED(hr) && !t.id.empty()) {  // refused (taken, gone just now): Windows' default
            t = output::Target{"", true};
            hr = devices::openMaster(d.x, &d.master, 2, "");
        }
    }
    ++d.openings;
    if (FAILED(hr)) {  // no output device at all: silent until one is connected
        if (d.x) d.x->Release();
        d.x = nullptr;
        d.master = nullptr;
        d.opened.clear();
        d.away = false;
        return false;
    }
    d.opened = t.id;
    d.away = t.away;
    return true;
}

Driver* openDriver() {
    auto* d = new (std::nothrow) Driver;
    if (!d) return nullptr;
    d->dll = LoadLibraryA("xaudio2_9.dll");
    if (d->dll && openEngine(*d, g_wanted)) {
        InitializeCriticalSection(&d->lock);
        d->quit = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        d->thread = d->quit ? CreateThread(nullptr, 0, feed, d, 0, nullptr) : nullptr;
        if (d->thread) return d;
        if (d->quit) CloseHandle(d->quit);
        DeleteCriticalSection(&d->lock);
        d->master->DestroyVoice();
    }
    if (d->x) d->x->Release();
    delete d;
    return nullptr;
}

// 3.0.4-beta.1: everything on a new engine on the game's thread - its device lost (XAudio2 stops for good), another
// chosen, or the chosen one back. The sounds playing end; the music goes on from where it was read.
void reopen(Driver& d) {
    EnterCriticalSection(&d.lock);
    for (Sample* s : d.samples) {
        if (s->voice) s->voice->DestroyVoice();
        s->voice = nullptr;
        s->started = false;
    }
    for (const Idle& i : d.idle) i.voice->DestroyVoice();
    d.idle.clear();
    for (Stream* s : d.streams) {
        if (s->voice) s->voice->DestroyVoice();
        s->voice = nullptr;
    }
    if (d.master) d.master->DestroyVoice();
    d.master = nullptr;
    if (d.x) d.x->Release();
    d.x = nullptr;
    g_letGo.clear();  // no voice reads them now
    if (openEngine(d, g_wanted))
        for (Stream* s : d.streams)
            if (s->started) s->started = newVoice(d, *s);
    LeaveCriticalSection(&d.lock);
}

// At the game's calls into it: a new engine when one was asked for
void service() {
    if (g_driver && InterlockedExchange(&g_reopen, 0)) reopen(*g_driver);
}

}  // namespace

extern "C" {

int __stdcall AIL_startup() { return 1; }

char* __stdcall AIL_set_redist_directory(const char* dir) {
    static char before[MAX_PATH];
    lstrcpynA(before, g_redist, MAX_PATH);
    if (dir) lstrcpynA(g_redist, dir, MAX_PATH);
    return before;
}

int __stdcall AIL_set_preference(unsigned number, int value) {
    if (number >= sizeof g_prefs / sizeof g_prefs[0]) return 0;
    const int was = g_prefs[number];
    g_prefs[number] = value;
    return was;
}

void* __stdcall AIL_open_digital_driver(unsigned, int, int, unsigned) {
    if (!g_driver) g_driver = openDriver();
    return g_driver;
}

// Everything of it stopped and gone; a handle the game still holds plays nothing more
void __stdcall AIL_close_digital_driver(void* h) {
    Driver* d = static_cast<Driver*>(h);
    if (!d || d != g_driver) return;
    SetEvent(d->quit);
    WaitForSingleObject(d->thread, 10000);  // 3.0.4-beta.1: Windows' list of devices answers within 5 s
    CloseHandle(d->thread);
    CloseHandle(d->quit);
    for (Stream* s : d->streams) {
        if (s->voice) s->voice->DestroyVoice();
        s->voice = nullptr;
        s->driver = nullptr;
    }
    for (Sample* s : d->samples) {
        if (s->voice) s->voice->DestroyVoice();
        s->voice = nullptr;
        s->started = false;
        s->driver = nullptr;
    }
    for (const Idle& i : d->idle) i.voice->DestroyVoice();
    if (d->master) d->master->DestroyVoice();  // 3.0.4-beta.1: none while no device would open
    if (d->x) d->x->Release();
    DeleteCriticalSection(&d->lock);
    delete d;
    g_driver = nullptr;
    g_letGo.clear();  // no voice reads them now
}

void __stdcall AIL_shutdown() {
    if (g_driver) AIL_close_digital_driver(g_driver);
}

void* __stdcall AIL_allocate_sample_handle(void* h) {
    Driver* d = static_cast<Driver*>(h);
    if (!d || d != g_driver) return nullptr;
    try {
        d->samples.push_back(nullptr);
        Sample* s = new Sample(Kind::Sample, d);
        d->samples.back() = s;
        return s;
    } catch (const std::bad_alloc&) {
        if (!d->samples.empty() && !d->samples.back()) d->samples.pop_back();
        return nullptr;
    }
}

void __stdcall AIL_release_sample_handle(void* h) { release(sampleOf(h, Kind::Sample)); }

// Miles' defaults again: full volume, the middle, stopped
void __stdcall AIL_init_sample(void* h) {
    Sample* s = sampleOf(h, Kind::Sample);
    if (!s) return;
    stop(*s);
    letGo(std::move(s->pcm.bytes));
    s->volume = 127;
    s->pan = 64;
}

int __stdcall AIL_set_sample_file(void* h, const void* image, int) { return setFile(sampleOf(h, Kind::Sample), image); }

void __stdcall AIL_set_sample_volume(void* h, int volume) {
    if (Sample* s = sampleOf(h, Kind::Sample)) {
        s->volume = std::clamp(volume, 0, 127);
        if (s->started) mix(*s);
    }
}

// Not taken by the game: the driver sets a 2D sound's side with it (0 left, 64 the middle, 127 right)
void __stdcall AIL_set_sample_pan(void* h, int pan) {
    if (Sample* s = sampleOf(h, Kind::Sample)) {
        s->pan = std::clamp(pan, 0, 127);
        if (s->started) mix(*s);
    }
}

void __stdcall AIL_start_sample(void* h) {
    service();
    if (Sample* s = sampleOf(h, Kind::Sample)) start(*s);
}

void __stdcall AIL_end_sample(void* h) {
    if (Sample* s = sampleOf(h, Kind::Sample)) stop(*s);
}

unsigned __stdcall AIL_sample_status(void* h) {
    service();
    return statusOf(sampleOf(h, Kind::Sample));
}

// One provider, the one the game looks for
int __stdcall AIL_enumerate_3D_providers(uint32_t* next, uint32_t* dest, char** name) {
    if (!next || *next != 0) return 0;
    *next = 1;
    if (dest) *dest = kProvider;
    if (name) *name = const_cast<char*>(kProviderName);
    return 1;
}

int __stdcall AIL_open_3D_provider(uint32_t provider) { return provider == kProvider ? 0 : 1; }  // M3D_NOERR
void __stdcall AIL_close_3D_provider(uint32_t) {}
int __stdcall AIL_3D_speaker_type(uint32_t) { return g_speakers; }
void __stdcall AIL_set_3D_speaker_type(uint32_t, int type) { g_speakers = type; }
int __stdcall AIL_3D_room_type(uint32_t) { return -1; }  // no rooms: the game sets none (0x421DA5)
void __stdcall AIL_set_3D_room_type(uint32_t, int) {}

void* __stdcall AIL_open_3D_listener(uint32_t provider) {
    if (provider != kProvider) return nullptr;
    g_listener = Listener{};
    return &g_listener;
}

void __stdcall AIL_close_3D_listener(void*) {}

void* __stdcall AIL_allocate_3D_sample_handle(uint32_t provider) {
    if (provider != kProvider || !g_driver) return nullptr;
    try {
        g_driver->samples.push_back(nullptr);
        Sample* s = new Sample(Kind::Sample3D, g_driver);
        g_driver->samples.back() = s;
        return s;
    } catch (const std::bad_alloc&) {
        if (!g_driver->samples.empty() && !g_driver->samples.back()) g_driver->samples.pop_back();
        return nullptr;
    }
}

void __stdcall AIL_release_3D_sample_handle(void* h) { release(sampleOf(h, Kind::Sample3D)); }

int __stdcall AIL_set_3D_sample_file(void* h, const void* image) { return setFile(sampleOf(h, Kind::Sample3D), image); }

void __stdcall AIL_set_3D_sample_volume(void* h, int volume) {
    if (Sample* s = sampleOf(h, Kind::Sample3D)) {
        s->volume = std::clamp(volume, 0, 127);
        if (s->started) mix(*s);
    }
}

void __stdcall AIL_set_3D_sample_distances(void* h, float farthest, float nearest) {
    if (Sample* s = sampleOf(h, Kind::Sample3D)) {
        s->farthest = farthest;
        s->nearest = nearest;
        if (s->started) mix(*s);
    }
}

void __stdcall AIL_set_3D_sample_effects_level(void*, float) {}  // no rooms, nothing to send to them

void __stdcall AIL_set_3D_position(void* h, float x, float y, float z) {
    service();
    if (kindOf(h) == Kind::Listener) {
        g_listener.pos[0] = x, g_listener.pos[1] = y, g_listener.pos[2] = z;
        placeAll();
    } else if (Sample* s = sampleOf(h, Kind::Sample3D)) {
        s->pos[0] = x, s->pos[1] = y, s->pos[2] = z;
        if (s->started) mix(*s);
    }
}

// The listener's facing (the camera's turn, 0x421DD0); a sample's own is of no matter (no cones)
void __stdcall AIL_set_3D_orientation(void* h, float fx, float fy, float fz, float ux, float uy, float uz) {
    service();
    if (kindOf(h) != Kind::Listener) return;
    g_listener.front[0] = fx, g_listener.front[1] = fy, g_listener.front[2] = fz;
    g_listener.up[0] = ux, g_listener.up[1] = uy, g_listener.up[2] = uz;
    placeAll();
}

void __stdcall AIL_start_3D_sample(void* h) {
    service();
    if (Sample* s = sampleOf(h, Kind::Sample3D)) start(*s);
}

void __stdcall AIL_end_3D_sample(void* h) {
    if (Sample* s = sampleOf(h, Kind::Sample3D)) stop(*s);
}

unsigned __stdcall AIL_3D_sample_status(void* h) {
    service();
    return statusOf(sampleOf(h, Kind::Sample3D));
}

void* __stdcall AIL_open_stream(void* h, const char* name, int) {
    service();
    Driver* d = static_cast<Driver*>(h);
    if (!d || d != g_driver || !name) return nullptr;
    Stream* s = nullptr;
    try {
        std::vector<uint8_t> file;
        if (!readWhole(name, file)) return nullptr;  // none, or empty
        s = new Stream(d);
        if (!s->music.open(std::move(file))) {
            delete s;
            return nullptr;
        }
    } catch (const std::bad_alloc&) {
        delete s;
        return nullptr;
    }
    bool kept = true;
    EnterCriticalSection(&d->lock);
    try {
        d->streams.push_back(s);
    } catch (const std::bad_alloc&) {
        kept = false;
    }
    LeaveCriticalSection(&d->lock);
    if (kept) return s;
    delete s;
    return nullptr;
}

void __stdcall AIL_close_stream(void* h) {
    Stream* s = streamOf(h);
    if (!s) return;
    if (Driver* d = s->driver) {
        EnterCriticalSection(&d->lock);
        d->streams.erase(std::remove(d->streams.begin(), d->streams.end(), s), d->streams.end());
        if (s->voice) s->voice->DestroyVoice();
        LeaveCriticalSection(&d->lock);
    }
    s->kind = Kind::None;
    delete s;
}

// From its start, on a voice of its own (the one before destroyed: XAudio2 is done with its buffers then)
void __stdcall AIL_start_stream(void* h) {
    service();
    Stream* s = streamOf(h);
    if (!s || !s->driver) return;
    Driver& d = *s->driver;
    EnterCriticalSection(&d.lock);
    if (s->voice) s->voice->DestroyVoice();
    s->voice = nullptr;
    s->started = false;
    s->paused = false;
    s->music.restart();
    s->started = newVoice(d, *s);
    LeaveCriticalSection(&d.lock);
}

void __stdcall AIL_pause_stream(void* h, int on) {
    Stream* s = streamOf(h);
    if (!s || !s->driver) return;
    EnterCriticalSection(&s->driver->lock);
    s->paused = on != 0;  // 3.0.4-beta.1: kept on a new engine's voice too
    if (s->voice && s->started) {
        if (on)
            s->voice->Stop(0, XAUDIO2_COMMIT_NOW);
        else
            s->voice->Start(0, XAUDIO2_COMMIT_NOW);
    }
    LeaveCriticalSection(&s->driver->lock);
}

void __stdcall AIL_set_stream_volume(void* h, int volume) {
    Stream* s = streamOf(h);
    if (!s) return;
    s->volume = std::clamp(volume, 0, 127);
    if (!s->driver) return;
    EnterCriticalSection(&s->driver->lock);
    if (s->voice) s->voice->SetVolume(miles::gainOf(s->volume), XAUDIO2_COMMIT_NOW);
    LeaveCriticalSection(&s->driver->lock);
}

int __stdcall AIL_stream_volume(void* h) {
    const Stream* s = streamOf(h);
    return s ? s->volume : 0;
}

void __stdcall AIL_set_stream_loop_count(void* h, int count) {
    Stream* s = streamOf(h);
    if (!s) return;
    if (s->driver) EnterCriticalSection(&s->driver->lock);
    s->music.setLoops(count);
    if (s->driver) LeaveCriticalSection(&s->driver->lock);
}

int __stdcall AIL_file_type(const void* data, unsigned size) {
    return miles::fileType(static_cast<const uint8_t*>(data), size);
}

int __stdcall AIL_WAV_info(const void* data, SoundInfo* info) {
    miles::SoundInfo s;
    const auto* image = static_cast<const uint8_t*>(data);
    if (!image || !info || !miles::wavInfo(image, miles::imageSize(image), s)) return 0;
    info->format = s.format;
    info->data = s.data;
    info->dataLen = s.dataLen;
    info->rate = s.rate;
    info->bits = s.bits;
    info->channels = s.channels;
    info->samples = s.samples;
    info->blockSize = s.blockSize;
    info->initial = s.initial;
    return 1;
}

// A 16-bit PCM WAVE image of an ADPCM one (IMA, and Microsoft's too); its size out where the game asks (it does not)
int __stdcall AIL_decompress_ADPCM(const SoundInfo* info, void** out, unsigned* size) {
    if (!info || !out) return 0;
    try {
        miles::SoundInfo s;
        const auto* image = static_cast<const uint8_t*>(info->initial);
        if (!image || !miles::wavInfo(image, miles::imageSize(image), s) || s.data != info->data) {
            s = miles::SoundInfo{};  // not the image WAV_info read: its fields alone
            s.format = info->format;
            s.data = static_cast<const uint8_t*>(info->data);
            s.dataLen = info->dataLen;
            s.rate = info->rate;
            s.bits = info->bits;
            s.channels = info->channels;
            s.blockSize = info->blockSize;
        }
        std::vector<int16_t> pcm;
        if (!miles::decodeAdpcm(s, pcm)) return 0;
        return give(miles::makeWav(pcm.data(), pcm.size() / size_t(s.channels), s.channels, s.rate), out, size);
    } catch (const std::bad_alloc&) {
        return 0;
    }
}

// The game's mp3 sounds (data\wav\effect\*.mp3) as 16-bit PCM WAVE images; mp3dec.asi is not needed
int __stdcall AIL_decompress_ASI(const void* in, unsigned insize, const char*, void** out, unsigned* size, void*) {
    if (!in || !insize || !out) return 0;
    try {
        std::vector<int16_t> pcm;
        int channels = 0;
        uint32_t rate = 0;
        if (!miles::decodeMp3(static_cast<const uint8_t*>(in), insize, pcm, channels, rate)) return 0;
        return give(miles::makeWav(pcm.data(), pcm.size() / size_t(channels), channels, rate), out, size);
    } catch (const std::bad_alloc&) {
        return 0;
    }
}

void __stdcall AIL_mem_free_lock(void* p) { std::free(p); }

// 3.0.4-beta.1, the driver's ([sound] output, win/sound_hook.cpp): the output device to play on - Windows' endpoint
// id, "" or NULL its default. Before the game opens its driver, or at once on a new engine.
void __stdcall UAGFX_set_output(const char* id) {
    const std::string wanted = id ? id : "";
    if (!g_driver) {
        g_wanted = wanted;
        return;
    }
    EnterCriticalSection(&g_driver->lock);
    const bool same = output::sameId(g_wanted, wanted);
    g_wanted = wanted;
    LeaveCriticalSection(&g_driver->lock);
    if (!same) reopen(*g_driver);
}

// For the driver's log, once a second: the device it plays on into `id` ("" Windows' default); its openings so far
// (0 none) with bit 31 set while the chosen one is not connected, bit 30 while no device would open. A new engine
// asked for is made first.
unsigned __stdcall UAGFX_output(char* id, unsigned size) {
    service();
    if (id && size) id[0] = '\0';
    if (!g_driver) return 0;
    EnterCriticalSection(&g_driver->lock);
    if (id && size) lstrcpynA(id, g_driver->opened.c_str(), int(size));
    const unsigned r = (g_driver->openings & 0x3FFFFFFFu) | (g_driver->away ? 0x80000000u : 0u) |
                       (g_driver->x ? 0u : 0x40000000u);
    LeaveCriticalSection(&g_driver->lock);
    return r;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        volatile const char* mark = kUaroMark;  // kept in the file
        (void)mark;
    }
    return TRUE;
}

}  // extern "C"
