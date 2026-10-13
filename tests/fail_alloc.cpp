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

void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
