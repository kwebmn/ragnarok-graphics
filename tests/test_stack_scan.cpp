// 3.0.20-beta.76: where a thread stands, from its stack's words (core/stack_scan.hpp) - the 64-bit renderer's hang
// watch writes it into the log when its thread stays in one message for seconds.
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "core/stack_scan.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

struct Memory {  // the images' bytes the test knows
    std::map<uint64_t, uint8_t> bytes;
    void put(uint64_t at, std::initializer_list<uint8_t> b) {
        for (const uint8_t v : b) bytes[at++] = v;
    }
};

bool readOf(void* context, uint64_t at, uint8_t* out, size_t size) {
    const Memory& m = *static_cast<const Memory*>(context);
    for (size_t i = 0; i < size; ++i) {
        const auto it = m.bytes.find(at + i);
        if (it == m.bytes.end()) return false;
        out[i] = it->second;
    }
    return true;
}

const uint64_t kA = 0x7FF600000000ull, kB = 0x7FFE12340000ull;

}  // namespace

TEST(stack_scan_keeps_the_words_that_follow_a_call) {
    Memory m;
    for (uint64_t at = kA; at < kA + 0x5000; ++at) m.bytes[at] = 0x90;
    for (uint64_t at = kB; at < kB + 0x400; ++at) m.bytes[at] = 0x90;
    m.put(kA + 0x1000, {0xE8, 0x10, 0x00, 0x00, 0x00});        // call rel32: returns to +0x1005
    m.put(kA + 0x2000, {0xFF, 0x15, 0x00, 0x10, 0x00, 0x00});  // call [rip+disp]: +0x2006
    m.put(kA + 0x3000, {0x41, 0xFF, 0xD0});                    // call r8: +0x3003
    m.put(kB + 0x100, {0xFF, 0x50, 0x18});                     // call [rax+0x18]: +0x103
    m.put(kB + 0x200, {0xFF, 0x90, 0x94, 0x00, 0x00, 0x00});   // call [rax+0x94]: +0x206
    const std::vector<stackscan::Module> modules = {{kA, 0x10000, "uaro-render64.exe"}, {kB, 0x8000, "nvngx_dlss.dll"}};
    const uint64_t words[] = {0x1234,           // a number
                              kA + 0x1005,      // a return
                              0xDEADBEEFCAFEull,
                              kA + 0x4000,      // a pointer into the image, no call before it: data
                              kB + 0x103, kA + 0x2006, kA + 0x3003,
                              kA + 0x20000,     // past the image's end
                              kB + 0x206, kB + 0x300};
    const std::vector<stackscan::Frame> f = stackscan::scan(words, 10, modules, readOf, &m, 16);
    CHECK_EQ(f.size(), size_t(5));
    if (f.size() == 5) {
        CHECK(f[0].module == 0 && f[0].rva == 0x1005);
        CHECK(f[1].module == 1 && f[1].rva == 0x103);
        CHECK(f[2].module == 0 && f[2].rva == 0x2006);
        CHECK(f[3].module == 0 && f[3].rva == 0x3003);
        CHECK(f[4].module == 1 && f[4].rva == 0x206);
    }
    // so many at most; no memory to read: none kept
    CHECK_EQ(stackscan::scan(words, 10, modules, readOf, &m, 2).size(), size_t(2));
    Memory none;
    CHECK(stackscan::scan(words, 10, modules, readOf, &none, 16).empty());
    CHECK(stackscan::scan(nullptr, 0, modules, readOf, &m, 16).empty());
}

TEST(stack_scan_tells_a_place_by_its_module) {
    const std::vector<stackscan::Module> modules = {{kA, 0x10000, "uaro-render64.exe"}, {kB, 0x8000, "nvngx_dlss.dll"}};
    CHECK_EQ(stackscan::place(kB + 0x1A2B, modules), std::string("nvngx_dlss.dll+0x1a2b"));
    CHECK_EQ(stackscan::place(kA, modules), std::string("uaro-render64.exe+0x0"));
    CHECK_EQ(stackscan::place(0x12345678ull, modules), std::string("0x12345678"));  // no module's
    const std::vector<stackscan::Frame> f = {{0, 0x1005}, {1, 0x103}};
    CHECK_EQ(stackscan::calls(f, modules), std::string("uaro-render64.exe+0x1005 < nvngx_dlss.dll+0x103"));
    CHECK_EQ(stackscan::calls({}, modules), std::string("none read"));
}
