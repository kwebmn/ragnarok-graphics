#pragma once
#include <string>
#include <vector>

namespace uagfx::win::audio_devices {

// 3.0.4-beta.1: Windows' output devices for [sound] output (core/audio_output.hpp) through its MMDevice API - ole32
// loaded by name (ddraw.dll imports none of it), each question on a thread of its own with COM there (whatever the
// caller's thread has). The driver, our mss32.dll and the settings app use it.

struct Device {
    std::string id;    // Windows' endpoint id: "{0.0.0.00000000}.{...}"
    std::string name;  // as Windows shows it (UTF-8): "Навушники (WH-1000XM4)"
};

// The output devices connected now and Windows' default one's id ("" none). False: Windows did not answer.
bool list(std::vector<Device>& active, std::string& defaultId);
// A device's name by its id, connected or not ("": not known to Windows)
std::string nameOf(const std::string& id);
// Grows with each of Windows' notices of output devices (one added, gone, turned on or off, a new default), watched
// on a thread of its own from the first call.
unsigned changes();
// An id as XAudio2 and Windows take it
std::wstring wide(const std::string& id);

}  // namespace uagfx::win::audio_devices
