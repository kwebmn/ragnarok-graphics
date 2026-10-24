#include "win/spatial_audio.hpp"

#include <windows.h>
#include <mmdeviceapi.h>
#include <propidl.h>
#include <spatialaudioclient.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/resample.hpp"
#include "win/audio_devices.hpp"
#include "win/log.hpp"

namespace uagfx::win::spatial {
namespace {

constexpr UINT32 kRate = 48000;
constexpr UINT32 kMostObjects = 32;

struct Playing {
    std::vector<uint8_t> data;
    wav::Sound sound;
    sound::Reader reader;
    ISpatialAudioObject* object = nullptr;
    float x = 0, y = 0, z = 0, gain = 1.0f;
};

using CoInitFn = HRESULT(WINAPI*)(void*, DWORD);
using CoUninitFn = void(WINAPI*)();
using CoCreateFn = HRESULT(WINAPI*)(REFCLSID, IUnknown*, DWORD, REFIID, void**);

struct State {
    CRITICAL_SECTION lock;
    bool started = false, ready = false, failed = false;
    volatile bool quit = false;  // 3.0.4-beta.1: stop()'s word to the thread
    bool com = false;            // its thread in COM (left at its end)
    std::string device;          // its output device ("" Windows' default)
    HANDLE thread = nullptr, event = nullptr, up = nullptr;
    ISpatialAudioObjectRenderStream* stream = nullptr;
    UINT32 objects = 0;
    std::vector<Playing*> pending, playing;
    volatile float master = 1.0f;
    char what[64] = "none";
    State() { InitializeCriticalSection(&lock); }
};

State& state() {
    static State s;
    return s;
}

// The output's spatial sound (the default's, 3.0.4-beta.1: or the chosen device's), activated for floats at 48 kHz on
// one channel: false when Windows has none on.
bool open(State& s) {
    HMODULE ole = LoadLibraryA("ole32.dll");
    const auto init = ole ? reinterpret_cast<CoInitFn>(reinterpret_cast<void*>(GetProcAddress(ole, "CoInitializeEx")))
                          : nullptr;
    const auto create = ole ? reinterpret_cast<CoCreateFn>(
                                  reinterpret_cast<void*>(GetProcAddress(ole, "CoCreateInstance")))
                            : nullptr;
    if (!init || !create) return false;
    s.com = SUCCEEDED(init(nullptr, COINIT_MULTITHREADED));  // this thread is the driver's own (left at its end)
    IMMDeviceEnumerator* devices = nullptr;
    IMMDevice* device = nullptr;
    ISpatialAudioClient* client = nullptr;
    HRESULT hr = create(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                        reinterpret_cast<void**>(&devices));
    const std::wstring id = audio_devices::wide(s.device);
    if (SUCCEEDED(hr))
        hr = id.empty() ? devices->GetDefaultAudioEndpoint(eRender, eConsole, &device)
                        : devices->GetDevice(id.c_str(), &device);
    if (SUCCEEDED(hr))
        hr = device->Activate(__uuidof(ISpatialAudioClient), CLSCTX_INPROC_SERVER, nullptr,
                              reinterpret_cast<void**>(&client));
    WAVEFORMATEX f{};
    f.wFormatTag = WAVE_FORMAT_IEEE_FLOAT;
    f.nChannels = 1;
    f.nSamplesPerSec = kRate;
    f.wBitsPerSample = 32;
    f.nBlockAlign = 4;
    f.nAvgBytesPerSec = kRate * 4;
    UINT32 most = 0;
    if (SUCCEEDED(hr)) hr = client->IsAudioObjectFormatSupported(&f);
    if (SUCCEEDED(hr)) hr = client->GetMaxDynamicObjectCount(&most);
    if (SUCCEEDED(hr) && most == 0) hr = E_FAIL;  // spatial sound turned off in Windows
    if (SUCCEEDED(hr)) {
        s.objects = std::min(most, kMostObjects);
        SpatialAudioObjectRenderStreamActivationParams p{};
        p.ObjectFormat = &f;
        p.StaticObjectTypeMask = AudioObjectType_None;
        p.MinDynamicObjectCount = 0;
        p.MaxDynamicObjectCount = s.objects;
        p.Category = AudioCategory_GameEffects;
        p.EventHandle = s.event;
        PROPVARIANT v;
        PropVariantInit(&v);
        v.vt = VT_BLOB;
        v.blob.cbSize = sizeof p;
        v.blob.pBlobData = reinterpret_cast<BYTE*>(&p);
        hr = client->ActivateSpatialAudioStream(&v, __uuidof(ISpatialAudioObjectRenderStream),
                                                reinterpret_cast<void**>(&s.stream));
    }
    if (SUCCEEDED(hr)) hr = s.stream->Start();
    if (client) client->Release();
    if (device) device->Release();
    if (devices) devices->Release();
    if (FAILED(hr)) {
        if (s.stream) s.stream->Release();
        s.stream = nullptr;
        LOG_INFO("spatial sound: not on in Windows (0x%08lx) - the engine's speakers' placing",
                 static_cast<unsigned long>(hr));
        return false;
    }
    wsprintfA(s.what, "Windows spatial sound, %u objects", s.objects);
    return true;
}

void release(Playing* p) {
    if (p->object) p->object->Release();
    delete p;
}

// 3.0.4-beta.1: the thread's end - the stream and its objects let go, COM left
void close(State& s) {
    for (Playing* p : s.playing) release(p);
    s.playing.clear();
    if (s.stream) {
        s.stream->Stop();
        s.stream->Release();
    }
    s.stream = nullptr;
    HMODULE ole = s.com ? GetModuleHandleA("ole32.dll") : nullptr;
    const auto uninit =
        ole ? reinterpret_cast<CoUninitFn>(reinterpret_cast<void*>(GetProcAddress(ole, "CoUninitialize"))) : nullptr;
    if (uninit) uninit();
    s.com = false;
}

DWORD WINAPI run(void*) {
    State& s = state();
    s.ready = open(s);
    s.failed = !s.ready;
    SetEvent(s.up);
    if (!s.ready) {
        close(s);
        return 0;
    }
    std::vector<float> scratch;
    for (;;) {
        if (s.quit) {
            close(s);
            return 0;
        }
        if (WaitForSingleObject(s.event, 200) != WAIT_OBJECT_0) continue;
        UINT32 free = 0, frames = 0;
        if (FAILED(s.stream->BeginUpdatingAudioObjects(&free, &frames))) {
            LOG_ERROR("spatial sound: the stream stopped - the engine's speakers' placing from now on");
            EnterCriticalSection(&s.lock);
            s.ready = false;
            LeaveCriticalSection(&s.lock);
            close(s);
            return 0;
        }
        EnterCriticalSection(&s.lock);
        for (Playing* p : s.pending) s.playing.push_back(p);
        s.pending.clear();
        LeaveCriticalSection(&s.lock);
        const float master = s.master;
        for (size_t i = 0; i < s.playing.size();) {
            Playing* p = s.playing[i];
            if (!p->object && (free == 0 || FAILED(s.stream->ActivateSpatialAudioObject(AudioObjectType_Dynamic,
                                                                                         &p->object)))) {
                p->object = nullptr;
                release(p);  // none free: dropped (rare - the engine asks only while there are)
                s.playing.erase(s.playing.begin() + long(i));
                continue;
            }
            if (!p->object) {
                ++i;
                continue;
            }
            if (free) --free;
            BYTE* buffer = nullptr;
            UINT32 bytes = 0;
            if (SUCCEEDED(p->object->GetBuffer(&buffer, &bytes)) && buffer) {
                const UINT32 n = std::min(frames, bytes / 4);
                auto* out = reinterpret_cast<float*>(buffer);
                const UINT32 got = p->reader.read(out, n);
                std::fill(out + got, out + n, 0.0f);
                p->object->SetPosition(p->x, p->y, p->z);
                p->object->SetVolume(p->gain * master);
                if (p->reader.done()) {
                    p->object->SetEndOfStream(got);
                    release(p);
                    s.playing.erase(s.playing.begin() + long(i));
                    continue;
                }
            }
            ++i;
        }
        s.stream->EndUpdatingAudioObjects();
    }
}

}  // namespace

bool start(const std::string& device) {
    State& s = state();
    if (s.started) return s.ready;
    s.started = true;
    s.quit = false;
    s.device = device;
    if (!s.event) s.event = CreateEventA(nullptr, FALSE, FALSE, nullptr);
    if (!s.up) s.up = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    if (s.up) ResetEvent(s.up);
    s.thread = s.event && s.up ? CreateThread(nullptr, 0, &run, nullptr, 0, nullptr) : nullptr;
    if (!s.thread) return false;
    WaitForSingleObject(s.up, 3000);
    if (s.ready) LOG_INFO("spatial sound: %s - the sounds placed in three dimensions by Windows", s.what);
    return s.ready;
}

void stop() {
    State& s = state();
    if (!s.started) return;
    EnterCriticalSection(&s.lock);
    s.ready = false;  // add() takes no more
    LeaveCriticalSection(&s.lock);
    s.quit = true;
    if (s.thread) {
        SetEvent(s.event);
        if (WaitForSingleObject(s.thread, 3000) == WAIT_OBJECT_0) {
            CloseHandle(s.thread);
        } else {  // stuck in Windows: left to itself, never started again
            LOG_ERROR("spatial sound: its thread did not stop - none from now on");
            return;
        }
    }
    s.thread = nullptr;
    EnterCriticalSection(&s.lock);
    for (Playing* p : s.pending) release(p);
    s.pending.clear();
    LeaveCriticalSection(&s.lock);
    s.started = false;
}

bool active() { return state().ready; }

bool add(const wav::Sound& snd, float azimuth, float meters, float gain, float cutoffHz) {
    State& s = state();
    if (!s.ready) return false;
    EnterCriticalSection(&s.lock);
    const bool room = s.pending.size() + s.playing.size() < s.objects;
    LeaveCriticalSection(&s.lock);
    if (!room) return false;
    auto* p = new Playing;
    p->data.assign(snd.data, snd.data + snd.bytes);
    p->sound = snd;
    p->sound.data = p->data.data();
    p->reader = sound::Reader(p->sound, kRate);
    p->reader.setCutoff(cutoffHz);
    // Windows' space: x right, y up, z towards the listener's back (ahead is -z); metres.
    const float a = azimuth * 0.01745329f, m = std::max(meters, 1.0f);
    p->x = std::sin(a) * m;
    p->y = 0.0f;
    p->z = -std::cos(a) * m;
    p->gain = gain;
    EnterCriticalSection(&s.lock);
    s.pending.push_back(p);
    LeaveCriticalSection(&s.lock);
    return true;
}

void setMaster(float gain) { state().master = gain; }

const char* name() { return state().what; }

}  // namespace uagfx::win::spatial
