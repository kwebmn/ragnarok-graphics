#include "win/sound_engine.hpp"

#include <windows.h>
#include <xaudio2.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include "core/sound_mix.hpp"
#include "core/sound_place.hpp"
#include "core/sound_space.hpp"
#include "core/wav.hpp"
#include "win/log.hpp"

namespace uagfx::win::audio {
namespace {

// XAudio2 2.9's reverberation parameters (xaudio2fx.h, Windows 10: with SideDelay and DisableLateField), packed.
#pragma pack(push, 1)
struct ReverbParameters {
    float WetDryMix;
    UINT32 ReflectionsDelay;
    BYTE ReverbDelay, RearDelay, SideDelay, PositionLeft, PositionRight, PositionMatrixLeft, PositionMatrixRight;
    BYTE EarlyDiffusion, LateDiffusion, LowEQGain, LowEQCutoff, HighEQGain, HighEQCutoff;
    float RoomFilterFreq, RoomFilterMain, RoomFilterHF, ReflectionsGain, ReverbGain, DecayTime, Density, RoomSize;
    BOOL DisableLateField;
};
#pragma pack(pop)

using CreateFn = HRESULT(WINAPI*)(IXAudio2** out, UINT32 flags, UINT32 processor);
using CreateVersionFn = HRESULT(WINAPI*)(IXAudio2** out, UINT32 flags, UINT32 processor, DWORD ntddi);
using ReverbFn = HRESULT(WINAPI*)(IUnknown** out);

// The player's place and the camera, as the game keeps them: the mode manager's game mode, its view's matrix (the
// mouse's ray is made with it: the screen's right its first row, where it looks its third).
constexpr uint32_t kModeKind = 0x74B5D0, kModeNow = 0x74B57C;
constexpr uint32_t kEffectsVolume = 0x70E814;  // the game's effects' volume (0-127), as FUN_00421af0 reads it
constexpr int kMaxVoices = 64;

struct Voice {
    IXAudio2SourceVoice* voice = nullptr;
    uint32_t key = 0;  // its format: channels, bits, rate
    std::vector<uint8_t> data;
    bool busy = false;
    float gain = 0.0f;
};

struct Engine {
    bool tried = false, on = false, wanted = false, logged = false, front = true, limiter = true;
    int reverbStrength = 60, background = 30;
    HMODULE dll = nullptr;
    IXAudio2* x = nullptr;
    IXAudio2MasteringVoice* master = nullptr;
    IXAudio2SubmixVoice* reverb = nullptr;
    DWORD mask = 0;
    UINT32 outChannels = 2, outRate = 48000;
    std::vector<float> angles;
    std::vector<Voice> voices;
    sound::Room room = sound::Room::Open;
    std::string map = "-";
    unsigned played = 0, refused = 0, stolen = 0;
};

Engine& engine() {
    static Engine e;
    return e;
}

uint32_t keyOf(const wav::Sound& s) { return uint32_t(s.channels) << 28 | uint32_t(s.bits) << 20 | s.rate; }

bool start(Engine& e) {
    e.tried = true;
    e.dll = LoadLibraryA("xaudio2_9.dll");
    const auto create =
        e.dll ? reinterpret_cast<CreateFn>(reinterpret_cast<void*>(GetProcAddress(e.dll, "XAudio2Create"))) : nullptr;
    const auto createVersion =  // newer Windows 10 has the SDK's own call too
        e.dll ? reinterpret_cast<CreateVersionFn>(
                    reinterpret_cast<void*>(GetProcAddress(e.dll, "XAudio2CreateWithVersionInfo")))
              : nullptr;
    if (!create && !createVersion) {
        LOG_INFO("sound engine: no XAudio2 2.9 (Windows 10 and later) - the game's sound through Miles");
        return false;
    }
    HRESULT hr = createVersion ? createVersion(&e.x, 0, 1, 0x0A000000) : create(&e.x, 0, 1);
    if (SUCCEEDED(hr)) hr = e.x->CreateMasteringVoice(&e.master, 0, 0, 0, nullptr, nullptr, AudioCategory_GameEffects);
    if (FAILED(hr)) {
        LOG_ERROR("sound engine: XAudio2 did not start (0x%08lx) - the game's sound through Miles",
                  static_cast<unsigned long>(hr));
        if (e.x) e.x->Release();
        e.x = nullptr;
        return false;
    }
    XAUDIO2_VOICE_DETAILS d{};
    e.master->GetVoiceDetails(&d);
    e.master->GetChannelMask(&e.mask);
    e.outChannels = d.InputChannels;
    e.outRate = d.InputSampleRate;
    e.angles = sound::speakers(e.mask);
    if (e.angles.size() != e.outChannels) e.angles.resize(e.outChannels, NAN);  // a mask that does not tell
    // The reverberation: a stereo submix with XAudio2's reverb on it, onto the speakers.
    const auto makeReverb =
        reinterpret_cast<ReverbFn>(reinterpret_cast<void*>(GetProcAddress(e.dll, "CreateAudioReverb")));
    IUnknown* apo = nullptr;
    if (makeReverb && SUCCEEDED(makeReverb(&apo)) && apo) {
        XAUDIO2_EFFECT_DESCRIPTOR fx{apo, TRUE, 2};
        XAUDIO2_EFFECT_CHAIN chain{1, &fx};
        if (FAILED(e.x->CreateSubmixVoice(&e.reverb, 2, e.outRate, 0, 0, nullptr, &chain))) e.reverb = nullptr;
        apo->Release();
    }
    LOG_INFO("sound engine: XAudio2 at %u Hz on %u channels (mask 0x%lx)%s", e.outRate, e.outChannels,
             static_cast<unsigned long>(e.mask), e.reverb ? ", reverberation" : ", no reverberation");
    e.on = true;
    return true;
}

// The place's reverberation (core/sound_place.hpp): how long and how much of it.
void setRoom(Engine& e, sound::Room room) {
    e.room = room;
    if (!e.reverb) return;
    ReverbParameters p{};
    p.WetDryMix = 100.0f;
    p.ReflectionsDelay = 5;
    p.ReverbDelay = 5;
    p.RearDelay = 5;
    p.SideDelay = 5;
    p.PositionLeft = p.PositionRight = 6;
    p.PositionMatrixLeft = p.PositionMatrixRight = 27;
    p.EarlyDiffusion = p.LateDiffusion = 8;
    p.LowEQGain = 8;
    p.LowEQCutoff = 4;
    p.HighEQGain = 8;
    p.HighEQCutoff = 4;
    p.RoomFilterFreq = 5000.0f;
    p.RoomFilterMain = 0.0f;
    p.RoomFilterHF = -6.0f;
    p.ReflectionsGain = 0.0f;
    p.ReverbGain = 0.0f;
    p.Density = 100.0f;
    switch (room) {
        case sound::Room::Cave:
            p.DecayTime = 2.6f, p.RoomSize = 90.0f, p.ReflectionsDelay = 20, p.ReverbDelay = 30;
            break;
        case sound::Room::Indoor: p.DecayTime = 0.7f, p.RoomSize = 30.0f; break;
        case sound::Room::Town: p.DecayTime = 1.1f, p.RoomSize = 60.0f, p.ReflectionsDelay = 12; break;
        case sound::Room::Open: p.DecayTime = 0.5f, p.RoomSize = 100.0f, p.ReverbGain = -12.0f; break;
    }
    if (FAILED(e.reverb->SetEffectParameters(0, &p, sizeof p, XAUDIO2_COMMIT_NOW))) {
        LOG_ERROR("sound engine: the reverberation's settings were refused - none");
        e.reverb->DestroyVoice();
        e.reverb = nullptr;
    }
}

float sendFor(const Engine& e) {
    const float room = e.room == sound::Room::Cave ? 0.45f : e.room == sound::Room::Indoor ? 0.28f
                       : e.room == sound::Room::Town ? 0.15f : 0.05f;
    return room * float(std::clamp(e.reverbStrength, 0, 100)) / 60.0f;
}

bool camera(float& rx, float& rz, float& fx, float& fz) {
    if (*reinterpret_cast<const int*>(uintptr_t(kModeKind)) != 1) return false;
    const uint32_t mode = *reinterpret_cast<const uint32_t*>(uintptr_t(kModeNow));
    const uint32_t view = mode ? *reinterpret_cast<const uint32_t*>(uintptr_t(mode + 0xC8)) : 0;
    if (!view) return false;
    const auto* m = reinterpret_cast<const float*>(uintptr_t(view + 0xC4));
    rx = m[0];
    rz = m[2];
    fx = m[6];
    fz = m[8];
    return std::isfinite(rx) && std::isfinite(rz) && std::isfinite(fx) && std::isfinite(fz);
}

// A voice free for this format: an idle one of it, a new one, or the quietest playing (stopped for it).
Voice* voiceFor(Engine& e, const wav::Sound& s) {
    const uint32_t key = keyOf(s);
    for (Voice& v : e.voices)
        if (!v.busy && v.key == key) return &v;
    Voice* victim = nullptr;
    if (int(e.voices.size()) >= kMaxVoices) {
        for (Voice& v : e.voices)
            if (!v.busy && (!victim || victim->busy)) victim = &v;
        if (!victim)
            for (Voice& v : e.voices)
                if (!victim || v.gain < victim->gain) victim = &v;
        if (!victim) return nullptr;
        victim->voice->Stop(0, XAUDIO2_COMMIT_NOW);
        victim->voice->FlushSourceBuffers();
        if (victim->busy) ++e.stolen;
        victim->busy = false;
        if (victim->key == key) return victim;
        victim->voice->DestroyVoice();
        victim->voice = nullptr;
    }
    WAVEFORMATEX f{};
    f.wFormatTag = WAVE_FORMAT_PCM;
    f.nChannels = s.channels;
    f.nSamplesPerSec = s.rate;
    f.wBitsPerSample = s.bits;
    f.nBlockAlign = s.blockAlign;
    f.nAvgBytesPerSec = s.rate * s.blockAlign;
    XAUDIO2_SEND_DESCRIPTOR sends[2] = {{0, e.master}, {0, e.reverb}};
    XAUDIO2_VOICE_SENDS list{e.reverb ? 2u : 1u, sends};
    IXAudio2SourceVoice* made = nullptr;
    if (FAILED(e.x->CreateSourceVoice(&made, &f, XAUDIO2_VOICE_USEFILTER, 2.0f, nullptr, &list, nullptr)))
        return nullptr;
    if (!victim) {
        e.voices.emplace_back();
        victim = &e.voices.back();
    }
    victim->voice = made;
    victim->key = key;
    return victim;
}

}  // namespace

void configure(bool on, int reverb, int background, bool limiter) {
    Engine& e = engine();
    e.wanted = on;
    e.limiter = limiter;
    e.reverbStrength = reverb;
    e.background = background;
}

bool play(const void* image, float x, float y, float z, int reach, int inner, float factor) {
    Engine& e = engine();
    if (!e.wanted) return false;
    if (!e.tried) start(e);
    if (!e.on) return false;
    wav::Sound s;
    const auto* bytes = static_cast<const uint8_t*>(image);
    uint32_t riff = 0;
    if (!bytes) return false;
    std::memcpy(&riff, bytes + 4, 4);
    if (!wav::parse(bytes, std::min<uint32_t>(riff + 8, 64u << 20), s) || !wav::playable(s)) {
        ++e.refused;
        return false;
    }
    float rx = 1, rz = 0, fx = 0, fz = 1;
    camera(rx, rz, fx, fz);  // no camera: as if looking north
    float azimuth = 0, distance = 0;
    sound::heard(x, z, rx, rz, fx, fz, azimuth, distance);
    distance = std::sqrt(distance * distance + y * y);
    const float outer = float(std::max(reach, 1)), full = float(std::max(inner, 1));
    const float master = float(std::clamp(*reinterpret_cast<const int*>(uintptr_t(kEffectsVolume)), 0, 127)) / 127.0f;
    int playing = 1;  // the crowd's limiter (core/sound_mix.hpp), as Miles' samples have it
    for (const Voice& o : e.voices) playing += o.busy ? 1 : 0;
    const float gain = master * sound::distanceGain(distance, full, outer) * (std::isfinite(factor) ? factor : 1.0f) *
                       float(e.limiter ? sound::crowdGain(playing) : 1.0);
    if (gain <= 0.001f) return true;  // out of reach, or the effects turned down: nothing, as the game
    Voice* v = voiceFor(e, s);
    if (!v) {
        ++e.refused;
        return false;
    }
    v->data.assign(s.data, s.data + s.bytes);
    // Each speaker's share (one row for each of the sound's channels: a stereo sound placed as one).
    float pan[16] = {};
    sound::pan(azimuth, e.angles, pan);
    std::vector<float> matrix(size_t(e.outChannels) * s.channels);
    for (UINT32 out = 0; out < e.outChannels && out < 16; ++out)
        for (UINT32 in = 0; in < s.channels; ++in)
            matrix[size_t(out) * s.channels + in] = pan[out] * (s.channels == 2 ? 0.7071f : 1.0f);
    v->voice->SetOutputMatrix(e.master, s.channels, e.outChannels, matrix.data(), XAUDIO2_COMMIT_NOW);
    if (e.reverb) {
        const float wet = sendFor(e) * std::sqrt(std::clamp(gain, 0.0f, 1.0f)) / std::max(gain, 0.05f);
        const float send[4] = {wet, wet, wet, wet};  // relative to the voice's own volume, the farther the more of it
        v->voice->SetOutputMatrix(e.reverb, s.channels, 2, send, XAUDIO2_COMMIT_NOW);
    }
    XAUDIO2_FILTER_PARAMETERS lp{LowPassFilter, std::min(1.0f, 2.0f * std::sin(3.14159265f *
                                     sound::muffleHz(distance, full, outer) / float(s.rate))), 1.0f};
    v->voice->SetFilterParameters(&lp, XAUDIO2_COMMIT_NOW);
    v->gain = gain;
    v->voice->SetVolume(gain, XAUDIO2_COMMIT_NOW);
    XAUDIO2_BUFFER b{};
    b.Flags = XAUDIO2_END_OF_STREAM;
    b.AudioBytes = uint32_t(v->data.size()) / s.blockAlign * s.blockAlign;
    b.pAudioData = v->data.data();
    if (FAILED(v->voice->SubmitSourceBuffer(&b, nullptr)) || FAILED(v->voice->Start(0, XAUDIO2_COMMIT_NOW))) {
        ++e.refused;
        return false;
    }
    v->busy = true;
    ++e.played;
    return true;
}

void frame(bool foreground, const std::string& map) {
    Engine& e = engine();
    if (!e.on) return;
    for (Voice& v : e.voices) {
        if (!v.busy) continue;
        XAUDIO2_VOICE_STATE st{};
        v.voice->GetState(&st, XAUDIO2_VOICE_NOSAMPLESPLAYED);
        if (st.BuffersQueued == 0) v.busy = false;
    }
    if (map != e.map) {
        e.map = map;
        setRoom(e, sound::roomOf(map));
    }
    if (foreground != e.front) {
        e.front = foreground;
        e.master->SetVolume(foreground ? 1.0f : float(std::clamp(e.background, 0, 100)) / 100.0f,
                            XAUDIO2_COMMIT_NOW);
    }
    static ULONGLONG last = 0;
    const ULONGLONG now = GetTickCount64();
    if (!e.logged && e.played) {
        e.logged = true;
        last = now;
        LOG_INFO("sound engine: the game's sounds placed around the player (%s, the place %d)",
                 e.outChannels == 2 ? "stereo" : e.outChannels == 6 ? "5.1" : e.outChannels == 8 ? "7.1" : "speakers",
                 int(e.room));
    } else if (e.logged && now - last > 60000) {
        last = now;
        LOG_INFO("sound engine: %u sounds played, %u left to Miles, %u stopped for newer ones in the last minute",
                 e.played, e.refused, e.stolen);
        e.played = e.refused = e.stolen = 0;
        e.logged = true;
    }
}

}  // namespace uagfx::win::audio
