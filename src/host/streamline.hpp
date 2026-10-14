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

    // mode: dlaa, quality, balanced, performance, ultra; generation: 0, else the frame multiplier 2-4 (DLSS frame
    // generation, with Reflex). Streamline's own log goes to logFolder (sl.log).
    bool init(const std::string& folder, const std::string& logFolder, const std::string& mode, int generation,
              std::string& error);
    HMODULE module() const;  // sl.interposer.dll: the renderer takes Direct3D 12 and DXGI from it (setApi)
    bool attach(ID3D12Device* device, std::string& error);  // false: this card or driver has no DLSS
    void setSigns(float jitter, float motion) {  // the test switches: -1 inverts
        jitterSign_ = jitter;
        motionSign_ = motion;
    }

    const char* name() const override { return name_; }
    bool renderSize(int outW, int outH, int& renderW, int& renderH) override;
    bool evaluate(const Frame& f) override;
    bool generates() const override { return generationReady_; }
    void renderStart() override;
    void beforePresent(const Generation& g) override;
    void afterPresent() override;  // after every present while it generates, whatever the frame did
    void suspend() override;

private:
    bool setOptions(int outW, int outH);
    void* token();  // this frame's sl::FrameToken: one for every Streamline call of a frame
    void marker(uint32_t m);  // sl::PCLMarker

    bool started_ = false;
    uint32_t mode_ = 0;  // sl::DLSSMode
    const char* name_ = "DLSS";
    uint32_t frame_ = 0;
    int optionsW_ = 0;  // the output size the options were set for
    int optionsH_ = 0;
    float jitterSign_ = 1.0f;
    float motionSign_ = 1.0f;
    int generation_ = 0;          // asked for: 2-4, 0 none
    bool generationReady_ = false;  // DLSS-G loaded and this card has it
    void* token_ = nullptr;       // sl::FrameToken* of the frame being drawn
    bool presentStarted_ = false;  // beforePresent ran this frame: its present has a start marker
    int generationMode_ = -1;     // the options set last: off (0) or on (1)
    uint32_t generationFrames_ = 1;
};

}  // namespace uagfx::host
