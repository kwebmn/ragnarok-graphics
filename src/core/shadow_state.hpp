#pragma once

namespace uagfx {

// Bookkeeping for the CPU copy of the render target that Lock and GetDC hand out. Reading the GPU target back
// stalls, so it happens only when the client really touches the memory: the client locks the target every frame
// without touching it (surfaces-present-caps.md §2). The Windows side protects the pages and calls touch() from
// its exception handler on the first read or write.
class ShadowState {
public:
    struct Touch {
        bool readBack;  // copy the GPU target into the memory first
        bool writable;  // open the pages for writing (otherwise read-only, so a later write is noticed)
    };

    void gpuChanged() { stale_ = true; }  // something was drawn or written into the target by other means
    void lock() {
        locked_ = true;
        dirty_ = false;
    }
    Touch touch(bool write);
    bool unlock();  // true when the client wrote: upload the memory into the target
    bool locked() const { return locked_; }
    bool stale() const { return stale_; }
    bool dirty() const { return dirty_; }

private:
    bool locked_ = false;
    bool stale_ = true;
    bool dirty_ = false;
};

}  // namespace uagfx
