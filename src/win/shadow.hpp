#pragma once
#include <windows.h>

#include <cstdint>
#include <vector>

#include "core/pixel.hpp"
#include "core/shadow_state.hpp"

namespace uagfx::win {

// The memory Lock and GetDC hand out for the render target, in the client's screen format. Its pages are made
// inaccessible on Lock; the first read or write traps into a vectored exception handler that reads the GPU target
// back first (ShadowState decides). The client locks the target every frame without touching it, so most locks
// cost nothing.
class Shadow {
public:
    ~Shadow();
    bool resize(int width, int height, PixelFormat format);
    int width() const { return w_; }
    int height() const { return h_; }
    int pitch() const { return pitch_; }
    PixelFormat format() const { return f_; }
    uint8_t* data() { return mem_; }

    uint8_t* lock();
    bool unlock();  // true when the client wrote: upload the memory into the target
    bool locked() const { return state_.locked(); }
    void gpuChanged() { state_.gpuChanged(); }

    void fill(const std::vector<uint32_t>& argb);    // GPU target -> memory
    void toArgb(std::vector<uint32_t>& argb) const;  // memory -> GPU target
    bool contains(const void* p) const;
    void fault(bool write);  // called by the exception handler, under the global lock

private:
    void protect(DWORD mode);

    uint8_t* mem_ = nullptr;
    size_t size_ = 0;
    int w_ = 0;
    int h_ = 0;
    int pitch_ = 0;
    PixelFormat f_ = PixelFormat::Unknown;
    ShadowState state_;
};

void installShadowHandler();  // once per process

}  // namespace uagfx::win
