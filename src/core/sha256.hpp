#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace uagfx::sha256 {

// 2.10: SHA-256 (FIPS 180-4) - the installer checks the site's archives by it. hex(): 64 lower-case hex digits; it
// ends the hash (call it once).
class Hasher {
public:
    Hasher();
    void add(const void* data, size_t n);
    std::string hex();

private:
    void block(const uint8_t* p);
    uint32_t h_[8];
    uint8_t buffer_[64];
    size_t used_ = 0;
    uint64_t total_ = 0;
};

std::string hexOf(const std::string& bytes);

}  // namespace uagfx::sha256
