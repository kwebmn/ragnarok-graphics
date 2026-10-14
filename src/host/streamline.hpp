#pragma once
#include <windows.h>

#include <cstdint>
#include <string>

#include "win/gpu/upscaler.hpp"

namespace uagfx::host {

// Part 3: DLSS through NVIDIA Streamline (third_party/streamline), its DLLs in uaro-graphics\dlss\. Streamline starts
// before the process makes any Direct3D call; the renderer's device is handed to it afterwards.
class Dlss : public gpu::Upscaler {
public:
    Dlss() = default;
    ~Dlss() override;
    Dlss(const Dlss&) = delete;
    Dlss& operator=(const Dlss&) = delete;

    // mode: dlaa, quality, balanced, performance, ultra. Streamline's own log goes to logFolder (sl.log).
    bool init(const std::string& folder, const std::string& logFolder, const std::string& mode, std::string& error);
    bool attach(ID3D12Device* device, std::string& error);  // false: this card or driver has no DLSS
    void setSigns(float jitter, float motion) {  // the test switches: -1 inverts
        jitterSign_ = jitter;
        motionSign_ = motion;
    }

    const char* name() const override { return name_; }
    bool renderSize(int outW, int outH, int& renderW, int& renderH) override;
    bool evaluate(const Frame& f) override;

private:
    bool setOptions(int outW, int outH);

    bool started_ = false;
    uint32_t mode_ = 0;  // sl::DLSSMode
    const char* name_ = "DLSS";
    uint32_t frame_ = 0;
    int optionsW_ = 0;  // the output size the options were set for
    int optionsH_ = 0;
    float jitterSign_ = 1.0f;
    float motionSign_ = 1.0f;
};

}  // namespace uagfx::host
