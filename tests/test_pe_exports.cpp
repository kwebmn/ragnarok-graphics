#include <cstring>

#include "core/pe_exports.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

// A loaded image as small as can be: DOS and PE headers, two sections (code, then data) and an export directory whose
// names come in their names' order, not their places', with a forwarder among them
struct Image {
    std::vector<uint8_t> bytes = std::vector<uint8_t>(0x3000, 0);
    void u16(uint32_t at, uint16_t v) { std::memcpy(&bytes[at], &v, 2); }
    void u32(uint32_t at, uint32_t v) { std::memcpy(&bytes[at], &v, 4); }
    void str(uint32_t at, const char* s) { std::memcpy(&bytes[at], s, std::strlen(s) + 1); }
    pe::Read reader() const {
        return [this](uint32_t rva, void* out, size_t n) {
            if (rva > bytes.size() || n > bytes.size() - rva) return false;
            std::memcpy(out, &bytes[rva], n);
            return true;
        };
    }
};

Image sample() {
    Image m;
    m.u16(0, 0x5A4D);        // MZ
    m.u32(0x3C, 0x80);       // the PE headers' place
    m.u32(0x80, 0x4550);     // PE\0\0
    m.u16(0x84 + 2, 2);      // two sections
    m.u16(0x84 + 16, 0xE0);  // the optional header's size
    const uint32_t opt = 0x98;
    m.u16(opt, 0x10B);        // PE32
    m.u32(opt + 92, 16);      // data directories
    m.u32(opt + 96, 0x2000);  // the exports'
    m.u32(opt + 100, 0x200);
    const uint32_t sec = opt + 0xE0;
    std::memcpy(&m.bytes[sec], ".text", 5);
    m.u32(sec + 8, 0x800);
    m.u32(sec + 12, 0x1000);
    m.u32(sec + 36, 0x60000020);  // code, executed, read
    std::memcpy(&m.bytes[sec + 40], ".rdata", 6);
    m.u32(sec + 40 + 8, 0x1000);
    m.u32(sec + 40 + 12, 0x2000);
    m.u32(sec + 40 + 36, 0x40000040);  // data, read
    m.u32(0x2000 + 20, 3);       // functions
    m.u32(0x2000 + 24, 3);       // names
    m.u32(0x2000 + 28, 0x2040);  // the functions' places
    m.u32(0x2000 + 32, 0x2060);  // the names
    m.u32(0x2000 + 36, 0x2080);  // the names' functions
    m.u32(0x2040, 0x1200);       // Sleep
    m.u32(0x2044, 0x1100);       // ReadFile
    m.u32(0x2048, 0x2100);       // in the export directory: a forwarder
    m.u32(0x2060, 0x20A0);
    m.u32(0x2064, 0x20B0);
    m.u32(0x2068, 0x20C0);
    m.u16(0x2080, 2);
    m.u16(0x2082, 1);
    m.u16(0x2084, 0);
    m.str(0x20A0, "EnterCriticalSection");
    m.str(0x20B0, "ReadFile");
    m.str(0x20C0, "Sleep");
    m.str(0x2100, "NTDLL.RtlEnterCriticalSection");
    return m;
}

}  // namespace

TEST(pe_exports_by_place_forwarders_left_out) {
    // 3.0.2-beta.12: a module's functions by their places - the one a place is in is the nearest at or before it
    const Image m = sample();
    const std::vector<pe::Export> e = pe::exportsOf(m.reader());
    CHECK(e.size() == 2);
    CHECK(e.size() == 2 && e[0].rva == 0x1100 && e[0].name == "ReadFile" && e[1].rva == 0x1200 &&
          e[1].name == "Sleep");
    CHECK(pe::exportAt(e, 0x1234) && pe::exportAt(e, 0x1234)->name == "Sleep");
    CHECK(pe::exportAt(e, 0x11ff) && pe::exportAt(e, 0x11ff)->name == "ReadFile");
    CHECK(pe::exportAt(e, 0x10ff) == nullptr);
}

TEST(pe_code_by_its_executable_sections) {
    const Image m = sample();
    uint32_t begin = 0, end = 0;
    CHECK(pe::codeOf(m.reader(), begin, end));
    CHECK(begin == 0x1000 && end == 0x1800);
}

TEST(pe_not_an_image) {
    const Image m;  // zeros
    uint32_t begin = 0, end = 0;
    CHECK(pe::exportsOf(m.reader()).empty());
    CHECK(!pe::codeOf(m.reader(), begin, end));
}

TEST(pe_exports_a_name_at_the_images_end) {
    // names are read 127 bytes at once; one too near the image's end for that, a character at a time
    Image m = sample();
    m.str(0x2FF9, "Sleep");
    m.u32(0x2068, 0x2FF9);
    const std::vector<pe::Export> e = pe::exportsOf(m.reader());
    CHECK(e.size() == 2 && e[1].name == "Sleep");
}

namespace {

// The image above with a third section: ".eh_fram" (as mingw's linker cuts ".eh_frame" to eight letters) - a CIE whose
// augmentation "zR" gives its FDEs' places as pcrel sdata4 (0x1b), and three FDEs (the last one's place before the
// first's), then the terminator
Image unwound() {
    Image m = sample();
    m.u16(0x84 + 2, 3);
    const uint32_t sec = 0x98 + 0xE0 + 80;
    std::memcpy(&m.bytes[sec], ".eh_fram", 8);
    m.u32(sec + 8, 0x200);
    m.u32(sec + 12, 0x2800);
    m.u32(sec + 36, 0x40000040);
    uint32_t at = 0x2800;
    const uint8_t cie[] = {0x01, 'z', 'R', 0x00, 0x01, 0x7c, 0x08, 0x01, 0x1b, 0x00, 0x00, 0x00};  // 12 bytes
    m.u32(at, 4 + sizeof cie);  // length
    m.u32(at + 4, 0);           // a CIE
    std::memcpy(&m.bytes[at + 8], cie, sizeof cie);
    const uint32_t cieAt = at;
    at += 8 + sizeof cie;
    const auto fde = [&](uint32_t begin, uint32_t size) {
        m.u32(at, 12);                       // length: the CIE pointer, the place, the size
        m.u32(at + 4, at + 4 - cieAt);       // back to the CIE
        m.u32(at + 8, begin - (at + 8));     // pcrel: from this field's own place
        m.u32(at + 12, size);
        at += 16;
    };
    fde(0x1100, 0x40);
    fde(0x1180, 0x100);
    fde(0x1000, 0x20);
    m.u32(at, 0);  // the end
    return m;
}

}  // namespace

TEST(pe_functions_from_the_unwind_table) {
    // 3.0.6-beta.3 (the game's thread on a FirePro laptop: the driver's share spread over 64-byte places): our own
    // module's functions by their unwind table (.eh_frame), so that the profile tallies samples by function
    const Image m = unwound();
    const std::vector<pe::Function> f = pe::functionsOf(m.reader());
    CHECK_EQ(int(f.size()), 3);
    CHECK(f.size() == 3 && f[0].rva == 0x1000 && f[0].size == 0x20 && f[1].rva == 0x1100 && f[2].rva == 0x1180 &&
          f[2].size == 0x100);
    CHECK(pe::functionAt(f, 0x1130) && pe::functionAt(f, 0x1130)->rva == 0x1100);
    CHECK(pe::functionAt(f, 0x1279) && pe::functionAt(f, 0x1279)->rva == 0x1180);
    CHECK(pe::functionAt(f, 0x1140) == nullptr);  // between two: none's
    CHECK(pe::functionAt(f, 0x0fff) == nullptr);
    CHECK(pe::functionsOf(sample().reader()).empty());  // no unwind table
}
