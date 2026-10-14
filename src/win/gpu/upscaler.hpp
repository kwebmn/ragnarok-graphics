#pragma once
#include <windows.h>

#include <d3d12.h>

namespace uagfx::gpu {

// Part 3: DLSS (uaro-render64.exe, through NVIDIA Streamline) for the renderer. The renderer draws the world at the
// upscaler's render size with a sub-pixel jitter, works out the motion of every pixel from the game's camera, and at
// each present asks for the world at the target's size; the interface goes on top afterwards. The renderer owns every
// texture; the upscaler records its work on the renderer's command list.
class Upscaler {
public:
    virtual ~Upscaler() = default;
    virtual const char* name() const = 0;  // "DLAA", "DLSS Quality", ...: for the log
    // The render size for an output of outW x outH; false when it cannot work at that size.
    virtual bool renderSize(int outW, int outH, int& renderW, int& renderH) = 0;

    struct Frame {
        ID3D12GraphicsCommandList* list = nullptr;
        ID3D12Resource* color = nullptr;   // render size: the world, jittered
        ID3D12Resource* depth = nullptr;   // render size, R32 (the game's depth, 0 near ... 1 far)
        ID3D12Resource* motion = nullptr;  // render size, R16G16: where each pixel was, minus where it is, in pixels
        ID3D12Resource* output = nullptr;  // output size, unordered access
        // 0.22.3, render size, R8: 1 where the history is not to be taken (characters, monsters, effects - they move
        // on their own, their motion is not the camera's); null: none.
        ID3D12Resource* bias = nullptr;
        D3D12_RESOURCE_STATES colorState{}, depthState{}, motionState{}, outputState{}, biasState{};  // left so
        int renderW = 0, renderH = 0, outW = 0, outH = 0;
        float jitter[2] = {0, 0};  // render pixels: how far the world's geometry was moved this frame
        bool reset = false;        // no history to use: the first frame, a new size, a camera jump
        // Row-major, row vectors, no jitter (Streamline's conventions).
        float viewToClip[16]{}, clipToView[16]{}, clipToPrevClip[16]{}, prevClipToClip[16]{};
        float nearZ = 0, farZ = 0, fovY = 0, aspect = 0;  // fovY in radians; aspect width / height
        float cameraPos[3]{}, cameraUp[3]{}, cameraRight[3]{}, cameraFwd[3]{};
    };
    // Records the upscale; false when it failed - the renderer then stretches the world itself. The command list's
    // state (heaps, root signature, pipeline, targets) is the renderer's to set again afterwards.
    virtual bool evaluate(const Frame& f) = 0;

    // Frame generation (DLSS-G), when it was asked for and the card has it: the frame's life around the present.
    struct Generation {
        ID3D12GraphicsCommandList* list = nullptr;  // submitted right before the present
        ID3D12Resource* hudless = nullptr;          // back buffer size: the picture without the interface
        ID3D12Resource* ui = nullptr;               // back buffer size: the interface, premultiplied
        D3D12_RESOURCE_STATES hudlessState{}, uiState{};
        RECT picture{};                             // where the picture is in the back buffer
        int backWidth = 0, backHeight = 0;
        bool on = false;  // generate around this frame: a world on screen with a known camera
    };
    virtual bool generates() const { return false; }
    virtual bool generating() const { return false; }   // on as of the last present: off takes a present to apply
    virtual void renderStart() {}                        // the frame's first commands arrived
    virtual void beforePresent(const Generation&) {}     // its inputs and options, before the present
    virtual void afterPresent() {}                       // the next frame begins (latency markers, sleep)
    // Generation off now: before the swap chain changes size, and for frames without the upscale (Pause).
    virtual void suspend() {}
};

}  // namespace uagfx::gpu
