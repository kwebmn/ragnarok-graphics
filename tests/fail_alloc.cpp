#include "fail_alloc.hpp"

#include <cstdlib>
#include <new>

namespace {
int g_allowed = -1;  // allocations left before they fail; -1: they never do
}  // namespace

namespace t {
FailAllocations::FailAllocations(int allowed) { g_allowed = allowed; }
FailAllocations::~FailAllocations() { g_allowed = -1; }
}  // namespace t

void* operator new(std::size_t size) {
    if (g_allowed == 0) throw std::bad_alloc();
    if (g_allowed > 0) --g_allowed;
    if (void* p = std::malloc(size ? size : 1)) return p;
    throw std::bad_alloc();
}

// Every other form on the same pair: nothing a sanitizer supplies frees what this one allocated (0.6.1 review I3).
void* operator new[](std::size_t size) { return operator new(size); }

void* operator new(std::size_t size, const std::nothrow_t&) noexcept {
    try {
        return operator new(size);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](std::size_t size, const std::nothrow_t& tag) noexcept { return operator new(size, tag); }

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete(void* p, const std::nothrow_t&) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, const std::nothrow_t&) noexcept { std::free(p); }
