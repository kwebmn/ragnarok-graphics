#pragma once

namespace t {

// While one lives, allocations fail: after `allowed` more of them, every one throws std::bad_alloc - a test sees what
// a structure is left with when memory runs out half-way (0.5.2 review 2). The test program's operator new - every
// form of it - is malloc, failing on request, and its operator delete is free (fail_alloc.cpp).
class FailAllocations {
public:
    explicit FailAllocations(int allowed);
    ~FailAllocations();
    FailAllocations(const FailAllocations&) = delete;
    FailAllocations& operator=(const FailAllocations&) = delete;
};

}  // namespace t
