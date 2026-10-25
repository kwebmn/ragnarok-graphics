#include "app/shots.hpp"

#include <windows.h>
#include <wincodec.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>

namespace uagfx::app {
namespace {

template <typename T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}

bool picture(const std::wstring& name) {
    std::wstring n = name;
    for (wchar_t& c : n)
        if (c >= L'A' && c <= L'Z') c = wchar_t(c - L'A' + L'a');
    for (const wchar_t* e : {L".jpg", L".jpeg", L".png", L".bmp"}) {
        const size_t l = wcslen(e);
        if (n.size() > l && n.compare(n.size() - l, l, e) == 0) return true;
    }
    return false;
}

// A picture as a JPEG of at most 1600 pixels across; "" when Windows cannot read it or make one.
std::string jpegOfSource(IWICImagingFactory* f, IWICBitmapSource* frame) {
    std::string out;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* bgr = nullptr;
    IStream* stream = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* target = nullptr;
    IPropertyBag2* props = nullptr;
    UINT w = 0, h = 0;
    if (SUCCEEDED(frame->GetSize(&w, &h)) && w && h) {
        const UINT tw = std::min<UINT>(w, 1600), th = std::max<UINT>(1, UINT(double(h) * tw / w));
        IWICBitmapSource* src = frame;
        if (tw != w && SUCCEEDED(f->CreateBitmapScaler(&scaler)) &&
            SUCCEEDED(scaler->Initialize(frame, tw, th, WICBitmapInterpolationModeFant)))
            src = scaler;
        if (SUCCEEDED(f->CreateFormatConverter(&bgr)) &&
            SUCCEEDED(bgr->Initialize(src, GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, nullptr, 0.0,
                                      WICBitmapPaletteTypeCustom)) &&
            SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) &&
            SUCCEEDED(f->CreateEncoder(GUID_ContainerFormatJpeg, nullptr, &enc)) &&
            SUCCEEDED(enc->Initialize(stream, WICBitmapEncoderNoCache)) &&
            SUCCEEDED(enc->CreateNewFrame(&target, &props))) {
            PROPBAG2 option{};
            option.pstrName = const_cast<LPOLESTR>(L"ImageQuality");
            VARIANT v{};
            v.vt = VT_R4;
            v.fltVal = 0.82f;
            props->Write(1, &option, &v);
            WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
            HGLOBAL mem = nullptr;
            if (SUCCEEDED(target->Initialize(props)) && SUCCEEDED(target->SetSize(tw, th)) &&
                SUCCEEDED(target->SetPixelFormat(&fmt)) && SUCCEEDED(target->WriteSource(bgr, nullptr)) &&
                SUCCEEDED(target->Commit()) && SUCCEEDED(enc->Commit()) &&
                SUCCEEDED(GetHGlobalFromStream(stream, &mem))) {
                STATSTG st{};
                if (SUCCEEDED(stream->Stat(&st, STATFLAG_NONAME))) {
                    const void* bytes = GlobalLock(mem);
                    if (bytes) out.assign(static_cast<const char*>(bytes), size_t(st.cbSize.QuadPart));
                    GlobalUnlock(mem);
                }
            }
        }
    }
    release(props);
    release(target);
    release(enc);
    release(stream);
    release(bgr);
    release(scaler);
    return out;
}

// 3.0.5-beta.5 (the user: «сделай так чтобы снимки делались без уменьшения качества»): a picture as a PNG of its own
// size, every pixel as it is; "" when Windows cannot make one.
std::string pngOfSource(IWICImagingFactory* f, IWICBitmapSource* frame) {
    std::string out;
    IWICFormatConverter* bgr = nullptr;
    IStream* stream = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* target = nullptr;
    IPropertyBag2* props = nullptr;
    UINT w = 0, h = 0;
    if (SUCCEEDED(frame->GetSize(&w, &h)) && w && h && SUCCEEDED(f->CreateFormatConverter(&bgr)) &&
        SUCCEEDED(bgr->Initialize(frame, GUID_WICPixelFormat24bppBGR, WICBitmapDitherTypeNone, nullptr, 0.0,
                                  WICBitmapPaletteTypeCustom)) &&
        SUCCEEDED(CreateStreamOnHGlobal(nullptr, TRUE, &stream)) &&
        SUCCEEDED(f->CreateEncoder(GUID_ContainerFormatPng, nullptr, &enc)) &&
        SUCCEEDED(enc->Initialize(stream, WICBitmapEncoderNoCache)) &&
        SUCCEEDED(enc->CreateNewFrame(&target, &props))) {
        WICPixelFormatGUID fmt = GUID_WICPixelFormat24bppBGR;
        HGLOBAL mem = nullptr;
        if (SUCCEEDED(target->Initialize(props)) && SUCCEEDED(target->SetSize(w, h)) &&
            SUCCEEDED(target->SetPixelFormat(&fmt)) && SUCCEEDED(target->WriteSource(bgr, nullptr)) &&
            SUCCEEDED(target->Commit()) && SUCCEEDED(enc->Commit()) && SUCCEEDED(GetHGlobalFromStream(stream, &mem))) {
            STATSTG st{};
            if (SUCCEEDED(stream->Stat(&st, STATFLAG_NONAME))) {
                const void* bytes = GlobalLock(mem);
                if (bytes) out.assign(static_cast<const char*>(bytes), size_t(st.cbSize.QuadPart));
                GlobalUnlock(mem);
            }
        }
    }
    release(props);
    release(target);
    release(enc);
    release(stream);
    release(bgr);
    return out;
}

std::string jpegOf(IWICImagingFactory* f, const std::wstring& path) {
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    std::string out;
    if (SUCCEEDED(f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                               &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame)))
        out = jpegOfSource(f, frame);
    release(frame);
    release(dec);
    return out;
}

IWICImagingFactory* factory() {
    IWICImagingFactory* f = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_IWICImagingFactory,
                                reinterpret_cast<void**>(&f))))
        return nullptr;
    return f;
}

// A decoded picture fitted into w x h on grey, as a 32-bit bitmap
HBITMAP thumbOf(IWICImagingFactory* f, IWICBitmapSource* frame, int w, int h) {
    UINT sw = 0, sh = 0;
    if (FAILED(frame->GetSize(&sw, &sh)) || !sw || !sh || w <= 0 || h <= 0) return nullptr;
    const double k = std::min(double(w) / sw, double(h) / sh);
    const UINT tw = std::max<UINT>(1, UINT(sw * k)), th = std::max<UINT>(1, UINT(sh * k));
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* bgra = nullptr;
    HBITMAP bmp = nullptr;
    BITMAPINFO bi{};
    bi.bmiHeader.biSize = sizeof bi.bmiHeader;
    bi.bmiHeader.biWidth = w;
    bi.bmiHeader.biHeight = -h;  // top-down
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biCompression = BI_RGB;
    void* bits = nullptr;
    if (SUCCEEDED(f->CreateBitmapScaler(&scaler)) &&
        SUCCEEDED(scaler->Initialize(frame, tw, th, WICBitmapInterpolationModeFant)) &&
        SUCCEEDED(f->CreateFormatConverter(&bgra)) &&
        SUCCEEDED(bgra->Initialize(scaler, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, nullptr, 0.0,
                                   WICBitmapPaletteTypeCustom)) &&
        (bmp = CreateDIBSection(nullptr, &bi, DIB_RGB_COLORS, &bits, nullptr, 0)) != nullptr) {
        uint32_t* px = static_cast<uint32_t*>(bits);
        for (int i = 0; i < w * h; ++i) px[i] = 0xFFE6E6E6;  // the list's grey around the picture, opaque
        const UINT left = (UINT(w) - tw) / 2, top = (UINT(h) - th) / 2;
        WICRect rect{0, 0, INT(tw), INT(th)};
        std::vector<uint32_t> row(size_t(tw) * th);
        if (SUCCEEDED(bgra->CopyPixels(&rect, tw * 4, UINT(row.size() * 4), reinterpret_cast<BYTE*>(row.data()))))
            for (UINT y = 0; y < th; ++y)
                std::copy(row.begin() + y * tw, row.begin() + (y + 1) * tw, px + (top + y) * UINT(w) + left);
    }
    release(bgra);
    release(scaler);
    return bmp;
}

}  // namespace

std::vector<std::wstring> gameShots(const std::wstring& gameDir, size_t most) {
    struct Found {
        std::wstring path;
        unsigned long long time;
    };
    std::vector<Found> found;
    FILETIME nowFt{};
    GetSystemTimeAsFileTime(&nowFt);
    const auto ticks = [](const FILETIME& t) { return (unsigned long long)t.dwHighDateTime << 32 | t.dwLowDateTime; };
    const unsigned long long now = ticks(nowFt), week = 7ull * 24 * 3600 * 10000000ull;
    const std::wstring dir = gameDir + L"\\ScreenShot";
    WIN32_FIND_DATAW d{};
    HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &d);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if ((d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !picture(d.cFileName)) continue;
            const unsigned long long t = ticks(d.ftLastWriteTime);
            if (now - t < week) found.push_back(Found{dir + L"\\" + d.cFileName, t});
        } while (FindNextFileW(h, &d));
        FindClose(h);
    }
    std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.time > b.time; });
    std::vector<std::wstring> out;
    for (const Found& x : found) {
        if (out.size() >= most) break;
        out.push_back(x.path);
    }
    return out;
}

std::string shotJpeg(const std::wstring& path) {
    IWICImagingFactory* f = factory();
    if (!f) return "";
    std::string jpeg = jpegOf(f, path);
    release(f);
    return jpeg.size() > 600000 ? std::string() : jpeg;  // the site takes a shot of 600 KB at most
}

std::string shotPng(const std::wstring& path) {
    std::string bytes;
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                           nullptr);
    if (h == INVALID_HANDLE_VALUE) return "";
    LARGE_INTEGER size{};
    if (GetFileSizeEx(h, &size) && size.QuadPart > 8 && size.QuadPart <= LONGLONG(kMostOriginal)) {
        bytes.resize(size_t(size.QuadPart));
        DWORD got = 0;
        if (!ReadFile(h, &bytes[0], DWORD(bytes.size()), &got, nullptr) || got != bytes.size()) bytes.clear();
    }
    CloseHandle(h);
    if (bytes.compare(0, 8, "\x89PNG\r\n\x1a\n", 8) == 0) return bytes;  // the game's own shots: sent as they are
    IWICImagingFactory* f = factory();  // a JPG or a BMP: its pixels into a PNG, nothing lost on the way
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    std::string png;
    if (f && SUCCEEDED(f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
                                                    WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame)))
        png = pngOfSource(f, frame);
    release(frame);
    release(dec);
    release(f);
    return png.size() > kMostOriginal ? std::string() : png;
}

std::string clipboardPng(HWND owner) {
    if (!IsClipboardFormatAvailable(CF_BITMAP) || !OpenClipboard(owner)) return "";
    std::string png;
    IWICImagingFactory* f = factory();
    IWICBitmap* bitmap = nullptr;
    HBITMAP hb = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
    if (f && hb && SUCCEEDED(f->CreateBitmapFromHBITMAP(hb, nullptr, WICBitmapIgnoreAlpha, &bitmap)))
        png = pngOfSource(f, bitmap);
    CloseClipboard();
    release(bitmap);
    release(f);
    return png.size() > kMostOriginal ? std::string() : png;
}

std::string clipboardJpeg(HWND owner) {
    if (!IsClipboardFormatAvailable(CF_BITMAP) || !OpenClipboard(owner)) return "";
    std::string jpeg;
    IWICImagingFactory* f = factory();
    IWICBitmap* bitmap = nullptr;
    HBITMAP hb = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
    if (f && hb && SUCCEEDED(f->CreateBitmapFromHBITMAP(hb, nullptr, WICBitmapIgnoreAlpha, &bitmap)))
        jpeg = jpegOfSource(f, bitmap);
    CloseClipboard();
    release(bitmap);
    release(f);
    return jpeg.size() > 600000 ? std::string() : jpeg;
}

HBITMAP shotThumb(const std::wstring& path, int w, int h) {
    IWICImagingFactory* f = factory();
    if (!f) return nullptr;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    HBITMAP bmp = nullptr;
    if (SUCCEEDED(f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                               &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame)))
        bmp = thumbOf(f, frame, w, h);
    release(frame);
    release(dec);
    release(f);
    return bmp;
}

HBITMAP jpegThumb(const std::string& jpeg, int w, int h) {
    IWICImagingFactory* f = factory();
    if (!f || jpeg.empty()) {
        release(f);
        return nullptr;
    }
    IWICStream* stream = nullptr;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    HBITMAP bmp = nullptr;
    if (SUCCEEDED(f->CreateStream(&stream)) &&
        SUCCEEDED(stream->InitializeFromMemory(reinterpret_cast<BYTE*>(const_cast<char*>(jpeg.data())),
                                               DWORD(jpeg.size()))) &&
        SUCCEEDED(f->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnDemand, &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame)))
        bmp = thumbOf(f, frame, w, h);
    release(frame);
    release(dec);
    release(stream);
    release(f);
    return bmp;
}

}  // namespace uagfx::app
