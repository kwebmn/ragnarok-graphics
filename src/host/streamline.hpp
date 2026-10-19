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
    // generation, with Reflex). Streamline's own log goes to logFolder (sl.log). late: Streamline comes after the
    // device (manual hooking), and only the DXGI factory goes through it (upgrade).
    bool init(const std::string& folder, const std::string& logFolder, const std::string& mode, int generation,
              bool late, std::string& error);
    HMODULE module() const;  // sl.interposer.dll: the renderer takes Direct3D 12 and DXGI from it (setApi)
    static bool upgrade(void** iface);  // slUpgradeInterface: a DXGI or Direct3D 12 object becomes its proxy
    bool attach(ID3D12Device* device, std::string& error);  // false: this card or driver has no DLSS
    void setSigns(float jitter, float motion) {  // the test switches: -1 inverts
        jitterSign_ = jitter;
        motionSign_ = motion;
    }
    // 2.4.2: mode custom - the share of the screen (%), or the size when written (0: by the share), drawn in the mode
    // whose range takes it (core/dlss_size.hpp); the network for every mode: auto, k, j, l or m. Before init.
    void setOwn(int percent, int width, int height, const std::string& model);
    // 2.5.26: [frames] reflex - NVIDIA Reflex without frame generation too (frame generation turns it on anyway).
    // Before init.
    void setReflex(bool on) { reflexWanted_ = on; }

    const char* name() const override { return own_ ? ownName_.c_str() : name_; }
    double shownPerFrame() override;  // 2.5.24
    int shownPerPresent() const override { return generationMode_ == 1 ? int(generationFrames_) + 1 : 1; }
    bool renderSize(int outW, int outH, int& renderW, int& renderH) override;
    bool evaluate(const Frame& f) override;
    bool generates() const override { return generationReady_; }
    bool generating() const override { return generationMode_ == 1; }
    bool latencyMarkers() const override { return reflexReady_; }  // 2.5.26
    void renderStart() override;
    void beforePresent(const Generation& g) override;
    void presentStart() override;  // 2.5.26
    void afterPresent() override;  // after every present while Reflex is on, whatever the frame did
    void suspend() override;

private:
    bool setOptions(int outW, int outH);
    bool ownSize(int outW, int outH, int& renderW, int& renderH);  // renderSize of mode custom
    void* token();  // this frame's sl::FrameToken: one for every Streamline call of a frame
    void marker(uint32_t m);  // sl::PCLMarker
    void startStats();         // frame generation came on
    void countStats();         // a frame with frame generation on
    void reportStats();        // the line since the last one: every half a minute and when it goes off
    void noteMode(const char* what);

    bool started_ = false;
    uint32_t mode_ = 0;  // sl::DLSSMode
    bool own_ = false;  // 2.4.2: mode custom (setOwn), mode_ the one that takes its size
    int ownPercent_ = 75, ownWidth_ = 0, ownHeight_ = 0;
    uint32_t preset_ = 0;  // 2.4.2: sl::DLSSPreset for every mode, 0 NVIDIA's for each
    std::string ownName_ = "DLSS";
    int ownForW_ = 0, ownForH_ = 0;  // the output its size was logged for
    const char* name_ = "DLSS";
    uint32_t frame_ = 0;
    int optionsW_ = 0;  // the output size the options were set for
    int optionsH_ = 0;
    float jitterSign_ = 1.0f;
    float motionSign_ = 1.0f;
    int generation_ = 0;          // asked for: 2-4, 0 none
    bool generationReady_ = false;  // DLSS-G loaded and this card has it
    bool reflexWanted_ = true;      // 2.5.26: [frames] reflex
    bool reflexReady_ = false;      // Reflex on: its markers and sleep every frame
    bool renderStarted_ = false;    // this frame's first markers set (renderStart)
    void* token_ = nullptr;       // sl::FrameToken* of the frame being drawn
    bool presentStarted_ = false;  // beforePresent ran this frame: its present has a start marker
    int generationMode_ = -1;     // the options set last: off (0) or on (1)
    uint32_t generationFrames_ = 1;
    bool framesKnown_ = false;  // generationFrames_ asked of the card
    bool statusLogged_ = false;
    ULONGLONG statsStart_ = 0;
    uint32_t statsFrames_ = 0;
    uint32_t statsShown_ = 0;
    uint32_t recentFrames_ = 0, recentShown_ = 0;  // 2.5.24: since the frame counter last asked
    int modeLines_ = 0;
};

}  // namespace uagfx::host
