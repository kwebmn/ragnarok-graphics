#include "win/sound_engine.hpp"

#include <windows.h>
#include <xaudio2.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <vector>

#include "core/ambience.hpp"
#include "core/audio_output.hpp"
#include "core/sound_mix.hpp"
#include "core/sound_place.hpp"
#include "core/sound_space.hpp"
#include "core/wav.hpp"
#include "win/audio_devices.hpp"
#include "win/ambience_files.hpp"
#include "win/audio_master.hpp"
#include "win/log.hpp"
#include "win/paths.hpp"
#include "win/spatial_audio.hpp"

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
// 3.0.4-beta.7: the game's sound mode (0 none) and its effects' box, as FUN_004396b0 asks them before a sound
constexpr uint32_t kSoundMode = 0x6B7B54, kEffectsOn = 0x6B8320;
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
    bool headphones = true;  // 1.2: Windows' spatial sound when it is on
    int reverbStrength = 60, background = 30;
    float others = 1.0f;  // 3.0.3-beta.9: frame()'s
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
    unsigned played = 0, refused = 0, stolen = 0, spatial = 0;
    // 3.0.4-beta.1: the output device asked for ("" Windows' default), the chosen one not connected, a new engine
    // asked for, Windows' notices of devices looked at
    std::string output;
    bool away = false, reopen = false, watching = false;
    unsigned seen = 0;
    volatile LONG lost = 0;  // XAudio2's critical error (its thread): the device gone, the engine stopped for good
    // 3.0.4-beta.7: the map's own sounds (core/ambience.hpp) - on, their volume (%), the map asked for, its sounds and
    // their files (the voices read the images), a voice for each file heard
    bool ambience = false;
    int ambienceVolume = 60;
    std::string ambienceMap = "-";
    ambience_files::MapSounds ambient;
    std::map<std::string, wav::Sound> ambientSounds;  // by file, over ambient.files' images
    struct AmbientVoice {
        std::string file;
        IXAudio2SourceVoice* voice = nullptr;
        uint32_t key = 0;
        uint64_t next = 0;  // when it sounds again
    };
    std::vector<AmbientVoice> ambientVoices;
    unsigned ambientPlayed = 0;
};

Engine& engine() {
    static Engine e;
    return e;
}

struct EngineWatch : IXAudio2EngineCallback {
    void STDMETHODCALLTYPE OnProcessingPassStart() override {}
    void STDMETHODCALLTYPE OnProcessingPassEnd() override {}
    void STDMETHODCALLTYPE OnCriticalError(HRESULT) override { InterlockedExchange(&engine().lost, 1); }
};
EngineWatch g_engineWatch;

// 3.0.4-beta.1: the ids of the output devices connected now; false when Windows did not answer
bool activeIds(std::vector<std::string>& ids) {
    std::vector<audio_devices::Device> active;
    std::string ignored;
    if (!audio_devices::list(active, ignored)) return false;
    for (const audio_devices::Device& d : active) ids.push_back(d.id);
    return true;
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
    audio_output::Target t;  // 3.0.4-beta.1: the chosen device when it is connected, else Windows' default
    if (!e.output.empty()) {
        std::vector<std::string> ids;
        if (!activeIds(ids)) ids.push_back(e.output);  // no answer: tried as it is
        t = audio_output::pick(e.output, ids);
    }
    HRESULT hr = createVersion ? createVersion(&e.x, 0, 1, 0x0A000000) : create(&e.x, 0, 1);
    if (SUCCEEDED(hr)) e.x->RegisterForCallbacks(&g_engineWatch);
    if (SUCCEEDED(hr)) {
        hr = audio_devices::openMaster(e.x, &e.master, 0, t.id);  // its channels: the device's
        if (FAILED(hr) && !t.id.empty()) {  // refused (taken, gone just now): Windows' default
            t = audio_output::Target{"", true};
            hr = audio_devices::openMaster(e.x, &e.master, 0, "");
        }
    }
    e.away = t.away;
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
    LOG_INFO("sound engine: XAudio2 at %u Hz on %u channels (mask 0x%lx)%s, %s", e.outRate, e.outChannels,
             static_cast<unsigned long>(e.mask), e.reverb ? ", reverberation" : ", no reverberation",
             !t.id.empty() ? "on the chosen output device"
             : t.away     ? "on Windows' default output (the chosen one is not connected)"
                          : "on Windows' default output");
    e.on = true;
    if (e.headphones) spatial::start(t.id);  // 1.2; 3.0.4-beta.1: on its device
    return true;
}

// 3.0.4-beta.1: everything of it let go - a new engine follows (its device lost, another chosen, the chosen one back)
void stop(Engine& e) {
    spatial::stop();
    for (Voice& v : e.voices)
        if (v.voice) v.voice->DestroyVoice();
    e.voices.clear();
    for (Engine::AmbientVoice& v : e.ambientVoices)  // 3.0.4-beta.7
        if (v.voice) v.voice->DestroyVoice();
    e.ambientVoices.clear();
    if (e.reverb) e.reverb->DestroyVoice();
    e.reverb = nullptr;
    if (e.master) e.master->DestroyVoice();
    e.master = nullptr;
    if (e.x) e.x->Release();
    e.x = nullptr;
    e.on = false;
    e.tried = false;
    e.map = "-";       // the place's reverberation set again
    e.others = -1.0f;  // and the volume
}

// While it plays on Windows' default instead of the chosen device (or not at all), Windows' notices of devices:
// the chosen one connected again, or any device now
bool deviceBack(Engine& e) {
    if (!e.away && e.on) {
        e.watching = false;
        return false;
    }
    if (!e.on && !e.dll) return false;  // no XAudio2 2.9 here
    const unsigned now = audio_devices::changes();  // the first call starts the watching
    if (!e.watching || now == e.seen) {
        e.watching = true;
        e.seen = now;
        return false;
    }
    e.seen = now;
    if (!e.on) return true;
    std::vector<std::string> ids;
    return activeIds(ids) && !audio_output::pick(e.output, ids).id.empty();
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

// 3.0.4-beta.7: where the camera looks - the character - in the world (CView+0x28, the maps' own places)
bool listenerAt(float at[3]) {
    if (*reinterpret_cast<const int*>(uintptr_t(kModeKind)) != 1) return false;
    const uint32_t mode = *reinterpret_cast<const uint32_t*>(uintptr_t(kModeNow));
    const uint32_t view = mode ? *reinterpret_cast<const uint32_t*>(uintptr_t(mode + 0xC8)) : 0;
    if (!view) return false;
    std::memcpy(at, reinterpret_cast<const void*>(uintptr_t(view + 0x28)), 12);
    return std::isfinite(at[0]) && std::isfinite(at[1]) && std::isfinite(at[2]);
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

std::string lowerName(std::string s) {
    for (char& c : s) c = c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c;
    return s;
}

void quietAmbient(Engine& e) {
    for (Engine::AmbientVoice& v : e.ambientVoices)
        if (v.voice) v.voice->SetVolume(0.0f, XAUDIO2_COMMIT_NOW);
}

// 3.0.4-beta.7 (the user's idea 6): the map's own sounds around the character - its .rsw's sound objects (read at
// the map's change, win/ambience_files.cpp), the nearest of each file within its reach, the loudest few
// (core/ambience.hpp): placed around the listener as the game's sounds are, each again once its last ended and its
// cycle passed; the game's effects' volume and box, [sound] ambience_volume
void ambienceFrame(Engine& e, const std::string& map, uint64_t now) {
    if (map != e.ambienceMap) {  // another map: its own sounds asked for, the old ones' voices let go first
        for (Engine::AmbientVoice& v : e.ambientVoices)
            if (v.voice) v.voice->DestroyVoice();
        e.ambientVoices.clear();
        e.ambientSounds.clear();
        e.ambient = ambience_files::MapSounds{};
        e.ambienceMap = map;
        if (!map.empty() && map != "-") ambience_files::ask(gameDir(), map);
    }
    ambience_files::MapSounds got;
    if (ambience_files::take(got) && got.map == e.ambienceMap) {
        e.ambient = std::move(got);
        for (auto& f : e.ambient.files) {
            wav::Sound s;
            if (wav::parse(f.second.data(), uint32_t(f.second.size()), s) && wav::playable(s))
                e.ambientSounds[f.first] = s;
        }
    }
    float at[3], rx = 1, rz = 0, fx = 0, fz = 1;
    const bool sounding = *reinterpret_cast<const int*>(uintptr_t(kSoundMode)) != 0 &&
                          *reinterpret_cast<const int*>(uintptr_t(kEffectsOn)) != 0;
    if (e.ambientSounds.empty() || !sounding || !listenerAt(at) || !camera(rx, rz, fx, fz)) {
        quietAmbient(e);
        return;
    }
    const float volume = float(std::clamp(*reinterpret_cast<const int*>(uintptr_t(kEffectsVolume)), 0, 127)) / 127.0f *
                         float(std::clamp(e.ambienceVolume, 0, 100)) / 100.0f;
    std::vector<bool> heard(e.ambientVoices.size(), false);
    for (const ambience::Heard& h : ambience::heard(e.ambient.sounds, at[0], at[1], at[2], rx, rz, fx, fz)) {
        const rsw::Sound& src = e.ambient.sounds[h.source];
        const std::string file = lowerName(src.file);
        const auto found = e.ambientSounds.find(file);
        if (found == e.ambientSounds.end()) continue;
        const wav::Sound& s = found->second;
        size_t i = 0;
        while (i < e.ambientVoices.size() && e.ambientVoices[i].file != file) ++i;
        if (i == e.ambientVoices.size()) {  // its voice, on the master and the place's reverberation
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
            if (FAILED(e.x->CreateSourceVoice(&made, &f, 0, 2.0f, nullptr, &list, nullptr))) continue;
            e.ambientVoices.push_back(Engine::AmbientVoice{file, made, keyOf(s), 0});
            heard.push_back(false);
        }
        Engine::AmbientVoice& v = e.ambientVoices[i];
        heard[i] = true;
        float pan[16] = {};
        sound::pan(h.azimuth, e.angles, pan);
        std::vector<float> matrix(size_t(e.outChannels) * s.channels);
        for (UINT32 out = 0; out < e.outChannels && out < 16; ++out)
            for (UINT32 in = 0; in < s.channels; ++in)
                matrix[size_t(out) * s.channels + in] = pan[out] * (s.channels == 2 ? 0.7071f : 1.0f);
        v.voice->SetOutputMatrix(e.master, s.channels, e.outChannels, matrix.data(), XAUDIO2_COMMIT_NOW);
        if (e.reverb) {
            const float wet = sendFor(e) * 0.5f, send[4] = {wet, wet, wet, wet};
            v.voice->SetOutputMatrix(e.reverb, s.channels, 2, send, XAUDIO2_COMMIT_NOW);
        }
        v.voice->SetVolume(h.gain * volume, XAUDIO2_COMMIT_NOW);
        if (now < v.next) continue;
        const uint64_t lengthMs = uint64_t(s.bytes) * 1000 / std::max<uint64_t>(uint64_t(s.rate) * s.blockAlign, 1);
        XAUDIO2_BUFFER b{};
        b.Flags = XAUDIO2_END_OF_STREAM;
        b.AudioBytes = s.bytes / s.blockAlign * s.blockAlign;
        b.pAudioData = s.data;
        v.voice->Stop(0, XAUDIO2_COMMIT_NOW);
        v.voice->FlushSourceBuffers();
        if (SUCCEEDED(v.voice->SubmitSourceBuffer(&b, nullptr)) && SUCCEEDED(v.voice->Start(0, XAUDIO2_COMMIT_NOW)))
            ++e.ambientPlayed;
        v.next = now + std::max<uint64_t>(lengthMs + 50, uint64_t(std::max(src.cycle, 0.5f) * 1000.0f));
    }
    for (size_t i = 0; i < e.ambientVoices.size(); ++i)
        if (!heard[i] && e.ambientVoices[i].voice) e.ambientVoices[i].voice->SetVolume(0.0f, XAUDIO2_COMMIT_NOW);
    static uint64_t said = 0;  // what they did, once a minute
    if (now - said > 60000 && e.ambientPlayed) {
        said = now;
        LOG_INFO("world sounds: %u played in the last minute on %s (%u files' voices)", e.ambientPlayed,
                 e.ambienceMap.c_str(), unsigned(e.ambientVoices.size()));
        e.ambientPlayed = 0;
    }
}

}  // namespace

void configureAmbience(bool on, int volume) {
    Engine& e = engine();
    if (e.ambience && !on) quietAmbient(e);
    e.ambience = on;
    e.ambienceVolume = volume;
}

int playing() {
    int n = 0;
    for (const Voice& v : engine().voices) n += v.busy ? 1 : 0;
    return n;
}

void setOutput(const std::string& id) {
    Engine& e = engine();
    if (audio_output::sameId(e.output, id)) return;
    e.output = id;
    e.reopen = e.tried;  // a running engine moves at the next frame
}

void configure(bool on, int reverb, int background, bool limiter, bool headphones) {
    Engine& e = engine();
    e.wanted = on;
    e.headphones = headphones;
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
    const float cutoff = sound::muffleHz(distance, full, outer);
    // 1.2: Windows' spatial sound has it - in three dimensions; the voice here then gives its echo alone
    const bool inSpace = e.headphones && spatial::active() && spatial::add(s, azimuth, distance / 5.0f, gain, cutoff);
    if (inSpace && !e.reverb) {
        ++e.played;
        ++e.spatial;
        return true;
    }
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
            matrix[size_t(out) * s.channels + in] = inSpace ? 0.0f : pan[out] * (s.channels == 2 ? 0.7071f : 1.0f);
    v->voice->SetOutputMatrix(e.master, s.channels, e.outChannels, matrix.data(), XAUDIO2_COMMIT_NOW);
    if (e.reverb) {
        const float wet = sendFor(e) * std::sqrt(std::clamp(gain, 0.0f, 1.0f)) / std::max(gain, 0.05f);
        const float send[4] = {wet, wet, wet, wet};  // relative to the voice's own volume, the farther the more of it
        v->voice->SetOutputMatrix(e.reverb, s.channels, 2, send, XAUDIO2_COMMIT_NOW);
    }
    XAUDIO2_FILTER_PARAMETERS lp{LowPassFilter, std::min(1.0f, 2.0f * std::sin(3.14159265f * cutoff / float(s.rate))),
                                 1.0f};
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
    e.spatial += inSpace ? 1 : 0;
    return true;
}

void frame(bool foreground, const std::string& map, float others) {
    Engine& e = engine();
    if (!e.tried && e.ambience) start(e);  // 3.0.4-beta.7: the world's sounds need it, the game's sounds or not
    if (e.tried && (e.wanted || e.ambience)) {  // 3.0.4-beta.1: a new engine - its device lost, another chosen, back
        const bool lost = InterlockedExchange(&e.lost, 0) != 0;
        if (lost || e.reopen || deviceBack(e)) {
            if (lost) LOG_INFO("sound engine: its output device is gone - opened again");
            e.reopen = false;
            stop(e);
            start(e);
        }
    }
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
    if (e.ambience) ambienceFrame(e, map, GetTickCount64());  // 3.0.4-beta.7
    if (foreground != e.front || std::fabs(others - e.others) > 0.01f) {
        e.front = foreground;
        e.others = others;
        const float level = (foreground ? 1.0f : float(std::clamp(e.background, 0, 100)) / 100.0f) * others;
        e.master->SetVolume(level, XAUDIO2_COMMIT_NOW);
        spatial::setMaster(level);  // 1.2
    }
    static ULONGLONG last = 0;
    const ULONGLONG now = GetTickCount64();
    if (!e.logged && e.played) {
        e.logged = true;
        last = now;
        LOG_INFO("sound engine: the game's sounds placed around the player (%s, the place %d)",
                 spatial::active() ? spatial::name()
                 : e.outChannels == 2 ? "stereo"
                 : e.outChannels == 6 ? "5.1"
                 : e.outChannels == 8 ? "7.1"
                                      : "speakers",
                 int(e.room));
    } else if (e.logged && now - last > 60000) {
        last = now;
        LOG_INFO("sound engine: %u sounds played (%u in Windows' spatial sound), %u left to Miles, %u stopped for "
                 "newer ones in the last minute",
                 e.played, e.spatial, e.refused, e.stolen);
        e.played = e.refused = e.stolen = e.spatial = 0;
        e.logged = true;
    }
}

}  // namespace uagfx::win::audio
