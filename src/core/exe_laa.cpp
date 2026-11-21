#include "core/exe_laa.hpp"

namespace uagfx::exelaa {
namespace {

constexpr uint16_t kLargeAddressAware = 0x20;

// The Characteristics' offset: after "PE\0\0" (4) and the file header's Machine..SizeOfOptionalHeader (18); 0 none
size_t fieldOf(const std::string& b) {
    if (b.size() < 0x40 || b[0] != 'M' || b[1] != 'Z') return 0;
    const size_t pe = size_t(uint8_t(b[0x3C])) | size_t(uint8_t(b[0x3D])) << 8 | size_t(uint8_t(b[0x3E])) << 16 |
                      size_t(uint8_t(b[0x3F])) << 24;
    if (pe + 24 > b.size() || b[pe] != 'P' || b[pe + 1] != 'E' || b[pe + 2] != 0 || b[pe + 3] != 0) return 0;
    return pe + 4 + 18;
}

}  // namespace

State stateOf(const std::string& exe) {
    const size_t at = fieldOf(exe);
    if (!at) return State::NotExe;
    return uint8_t(exe[at]) & kLargeAddressAware ? State::Set : State::Clear;
}

bool set(std::string& exe, bool on) {
    const size_t at = fieldOf(exe);
    if (!at) return false;
    exe[at] = char(on ? uint8_t(exe[at]) | kLargeAddressAware : uint8_t(exe[at]) & ~kLargeAddressAware);
    return true;
}

}  // namespace uagfx::exelaa
