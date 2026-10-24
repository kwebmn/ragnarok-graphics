#pragma once
#include <windows.h>
#include <xaudio2.h>

#include <string>

#include "win/audio_devices.hpp"

namespace uagfx::win::audio_devices {

// 3.0.4-beta.1: an XAudio2 mastering voice for the game's sound on the output device `id` (Windows' endpoint id, ""
// its default): by the endpoint's id, then by its device interface path ("\\?\SWD#MMDEVAPI#<id>#{e6327cad-...}", the
// form Windows.Devices.Enumeration gives) - which of the two Windows' own XAudio2 2.9 takes is not written down.
inline HRESULT openMaster(IXAudio2* x, IXAudio2MasteringVoice** master, UINT32 channels, const std::string& id) {
    if (id.empty()) return x->CreateMasteringVoice(master, channels, 0, 0, nullptr, nullptr, AudioCategory_GameEffects);
    const std::wstring endpoint = wide(id);
    HRESULT hr = x->CreateMasteringVoice(master, channels, 0, 0, endpoint.c_str(), nullptr, AudioCategory_GameEffects);
    if (FAILED(hr)) {
        const std::wstring path = L"\\\\?\\SWD#MMDEVAPI#" + endpoint + L"#{e6327cad-dcec-4949-ae8a-991e976a79d2}";
        hr = x->CreateMasteringVoice(master, channels, 0, 0, path.c_str(), nullptr, AudioCategory_GameEffects);
    }
    return hr;
}

}  // namespace uagfx::win::audio_devices
