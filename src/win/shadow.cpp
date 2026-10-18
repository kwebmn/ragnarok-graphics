#include "win/shadow.hpp"

#include "win/context.hpp"
#include "win/log.hpp"

namespace uagfx::win {
namespace {

LONG CALLBACK onFault(EXCEPTION_POINTERS* e) {
    const EXCEPTION_RECORD* r = e->ExceptionRecord;
    if (r->ExceptionCode != EXCEPTION_ACCESS_VIOLATION || r->NumberParameters < 2) return EXCEPTION_CONTINUE_SEARCH;
    const void* address = reinterpret_cast<const void*>(r->ExceptionInformation[1]);
    Context& c = Context::get();
    // Other modules' access violations (handled by their own SEH) must not wait for our lock: check the address
    // first, then take the lock and check again.
    if (!c.shadow.contains(address)) return EXCEPTION_CONTINUE_SEARCH;
    Guard guard;
    if (!c.shadow.contains(address) || !c.shadow.locked()) return EXCEPTION_CONTINUE_SEARCH;
    c.shadow.fault(r->ExceptionInformation[0] == 1);
    return EXCEPTION_CONTINUE_EXECUTION;
}

}  // namespace

void installShadowHandler() {
    static bool installed = false;
    if (installed) return;
    installed = AddVectoredExceptionHandler(1, onFault) != nullptr;
    if (!installed) LOG_ERROR("cannot install the exception handler: every Lock of the target will read it back");
}

Shadow::~Shadow() {
    if (mem_) VirtualFree(mem_, 0, MEM_RELEASE);
}

bool Shadow::resize(int width, int height, PixelFormat format) {
    if (mem_) VirtualFree(mem_, 0, MEM_RELEASE);
    mem_ = nullptr;
    w_ = width;
    h_ = height;
    f_ = format;
    pitch_ = (width * bytesPerPixel(format) + 3) & ~3;
    size_ = (size_t(pitch_) * size_t(height) + 4095) & ~size_t(4095);
    state_ = ShadowState{};
    mem_ = static_cast<uint8_t*>(VirtualAlloc(nullptr, size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    return mem_ != nullptr;
}

void Shadow::protect(DWORD mode) {
    DWORD old = 0;
    if (mem_) VirtualProtect(mem_, size_, mode, &old);
}

uint8_t* Shadow::lock() {
    state_.lock();
    protect(PAGE_NOACCESS);
    return mem_;
}

bool Shadow::unlock() {
    const bool written = state_.unlock();
    protect(PAGE_READWRITE);
    return written;
}

bool Shadow::contains(const void* p) const {
    const uint8_t* b = static_cast<const uint8_t*>(p);
    return mem_ && b >= mem_ && b < mem_ + size_;
}

void Shadow::fault(bool write) {
    Context& c = Context::get();
    c.flush();  // what was recorded before the touch is part of what the client reads
    const ShadowState::Touch t = state_.touch(write);
    if (t.readBack) {
        protect(PAGE_READWRITE);
        std::vector<uint32_t> argb;
        if (c.readTarget(argb)) fill(argb);  // the record is already empty: no second flush
    }
    protect(t.writable ? PAGE_READWRITE : PAGE_READONLY);
}

void Shadow::fill(const std::vector<uint32_t>& argb) {
    if (argb.size() < size_t(w_) * size_t(h_)) return;
    for (int y = 0; y < h_; ++y)  // 2.5.11: a row at a time
        rowTargetToClient(f_, argb.data() + size_t(y) * size_t(w_), mem_ + size_t(y) * size_t(pitch_), w_);
}

void Shadow::toArgb(std::vector<uint32_t>& argb) const {
    argb.resize(size_t(w_) * size_t(h_));
    for (int y = 0; y < h_; ++y)  // 2.5.11: a row at a time
        rowToArgb8(f_, mem_ + size_t(y) * size_t(pitch_), argb.data() + size_t(y) * size_t(w_), w_);
}

}  // namespace uagfx::win
