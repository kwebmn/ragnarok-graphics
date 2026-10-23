// 3.0.1-beta.6 (the user: our own libraries in place of the client's old ones): ijl15.dll - Intel JPEG Library 1.5
// as uaRO.exe takes it (ordinals 2 ijlInit, 3 ijlFree, 4 ijlRead, 5 ijlWrite; stdcall): its loader (0x42BF50) reads
// a JPEG from memory (modes 1, then 3), its Print Screen (0x411F72) writes a file (mode 8). The work: core/ijl.
#include <windows.h>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <new>
#include <vector>

#include "core/ijl.hpp"

namespace {

namespace ijl = uagfx::ijl;

// IJL 1.5's JPEG_CORE_PROPERTIES, the fields the library reads and fills: the game's struct is 0x4E68 bytes, zeroed
// before ijlInit; the rest of it is IJL's own JPEG_PROPERTIES, which nobody else reads
struct CoreProps {
    DWORD useJpegProperties;  // 0x00
    BYTE* dibBytes;           // 0x04
    DWORD dibWidth;           // 0x08
    int dibHeight;            // 0x0C: above 0 the top row first
    DWORD dibPadBytes;        // 0x10
    DWORD dibChannels;        // 0x14
    int dibColor;             // 0x18: 1 RGB, 2 BGR
    int dibSubsampling;       // 0x1C
    const char* jpgFile;      // 0x20
    BYTE* jpgBytes;           // 0x24
    DWORD jpgSizeBytes;       // 0x28
    DWORD jpgWidth;           // 0x2C
    DWORD jpgHeight;          // 0x30
    DWORD jpgChannels;        // 0x34
    int jpgColor;             // 0x38: 3 YCbCr, 4 grey
    int jpgSubsampling;       // 0x3C: 1 4:1:1
    DWORD jpgThumbWidth;      // 0x40
    DWORD jpgThumbHeight;     // 0x44
    DWORD cconversionReqd;    // 0x48
    DWORD upsamplingReqd;     // 0x4C
    DWORD jquality;           // 0x50
};
static_assert(offsetof(CoreProps, dibColor) == 0x18 && offsetof(CoreProps, jpgFile) == 0x20 &&
                  offsetof(CoreProps, jpgBytes) == 0x24 && offsetof(CoreProps, jpgWidth) == 0x2C &&
                  offsetof(CoreProps, jquality) == 0x50,
              "IJL 1.5's JPEG_CORE_PROPERTIES");

// IJLIOTYPE's
enum Mode : int {
    kFileParams = 0, kBufferParams = 1, kFileImage = 2, kBufferImage = 3, kFileHeader = 4, kBufferHeader = 5,
    kFileWrite = 8, kBufferWrite = 9
};

ijl::Dib dibOf(const CoreProps& p) {
    ijl::Dib d;
    d.bytes = p.dibBytes;
    d.width = int(p.dibWidth);
    d.height = p.dibHeight;
    d.padBytes = int(p.dibPadBytes);
    d.channels = int(p.dibChannels);
    d.color = p.dibColor;
    return d;
}

bool readWhole(const char* name, std::vector<uint8_t>& out) {
    std::FILE* f = name ? std::fopen(name, "rb") : nullptr;
    if (!f) return false;
    uint8_t buf[65536];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return !out.empty();
}

int read(CoreProps* p, int mode) {
    std::vector<uint8_t> file;
    const uint8_t* jpg = p->jpgBytes;
    size_t size = p->jpgSizeBytes;
    if (mode == kFileParams || mode == kFileImage || mode == kFileHeader) {
        if (!readWhole(p->jpgFile, file)) return ijl::kInvalidFilename;
        jpg = file.data();
        size = file.size();
    }
    if (mode == kFileImage || mode == kBufferImage) return ijl::readImage(jpg, size, dibOf(*p));
    if (mode != kFileParams && mode != kBufferParams && mode != kFileHeader && mode != kBufferHeader)
        return ijl::kInvalidProperties;
    ijl::Params q;
    const int r = ijl::readParams(jpg, size, q);
    if (r != ijl::kOk) return r;
    p->jpgWidth = DWORD(q.width);
    p->jpgHeight = DWORD(q.height);
    p->jpgChannels = DWORD(q.channels);
    p->jpgColor = q.channels == 1 ? 4 : 3;
    return ijl::kOk;
}

int write(CoreProps* p, int mode) {
    if (mode != kFileWrite && mode != kBufferWrite) return ijl::kInvalidProperties;
    std::vector<uint8_t> jpg;
    const int r = ijl::writeImage(dibOf(*p), int(p->jquality), jpg);
    if (r != ijl::kOk) return r;
    if (mode == kBufferWrite) {
        if (!p->jpgBytes || jpg.size() > p->jpgSizeBytes) return ijl::kBufferTooSmall;
        std::memcpy(p->jpgBytes, jpg.data(), jpg.size());
        p->jpgSizeBytes = DWORD(jpg.size());
        return ijl::kOk;
    }
    std::FILE* f = p->jpgFile ? std::fopen(p->jpgFile, "wb") : nullptr;
    if (!f) return p->jpgFile ? ijl::kFileError : ijl::kInvalidFilename;
    const bool ok = std::fwrite(jpg.data(), 1, jpg.size(), f) == jpg.size();
    return std::fclose(f) == 0 && ok ? ijl::kOk : ijl::kFileError;
}

}  // namespace

extern "C" {

// Told from Intel's ijl15.dll by these bytes (core/client_libs.hpp)
__attribute__((used)) const char kUaroMark[] = "uaRO Graphics client library: ijl15.dll (JPEG over stb)";

struct LibVersion {  // IJLibVersion
    int major, minor, build;
    const char *name, *version, *internalVersion, *buildDate, *callConv;
};

const LibVersion* __stdcall ijlGetLibVersion() {
    static const LibVersion v = {1, 51, 1, "ijl15.dll of uaRO Graphics", "1.51", "stb_image", __DATE__, "stdcall"};
    return &v;
}

// IJL's defaults (the game zeroes the struct first and sets what it needs after)
int __stdcall ijlInit(CoreProps* p) {
    if (!p) return ijl::kInvalidProperties;
    p->dibChannels = 3;
    p->dibColor = ijl::kBgr;
    p->jpgChannels = 3;
    p->jpgColor = 3;
    p->jpgSubsampling = 1;
    p->cconversionReqd = 1;
    p->upsamplingReqd = 1;
    p->jquality = 75;
    return ijl::kOk;
}

int __stdcall ijlFree(CoreProps* p) { return p ? ijl::kOk : ijl::kInvalidProperties; }

int __stdcall ijlRead(CoreProps* p, int mode) {
    if (!p) return ijl::kInvalidProperties;
    try {
        return read(p, mode);
    } catch (const std::bad_alloc&) {  // never into the game's code
        return ijl::kMemory;
    }
}

int __stdcall ijlWrite(CoreProps* p, int mode) {
    if (!p) return ijl::kInvalidProperties;
    try {
        return write(p, mode);
    } catch (const std::bad_alloc&) {
        return ijl::kMemory;
    }
}

const char* __stdcall ijlErrorStr(int code) { return ijl::errorText(code); }

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        volatile const char* mark = kUaroMark;  // kept in the file
        (void)mark;
    }
    return TRUE;
}

}  // extern "C"
