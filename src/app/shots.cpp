#include "app/shots.hpp"

#include <windows.h>
#include <shlobj.h>
#include <wincodec.h>

#include <algorithm>
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

// The picture as a JPEG of at most 1600 pixels across; "" when Windows cannot read it.
std::string jpegOf(IWICImagingFactory* f, const std::wstring& path) {
    std::string out;
    IWICBitmapDecoder* dec = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICBitmapScaler* scaler = nullptr;
    IWICFormatConverter* bgr = nullptr;
    IStream* stream = nullptr;
    IWICBitmapEncoder* enc = nullptr;
    IWICBitmapFrameEncode* target = nullptr;
    IPropertyBag2* props = nullptr;
    UINT w = 0, h = 0;
    if (SUCCEEDED(f->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand,
                                               &dec)) &&
        SUCCEEDED(dec->GetFrame(0, &frame)) && SUCCEEDED(frame->GetSize(&w, &h)) && w && h) {
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
    release(frame);
    release(dec);
    return out;
}

}  // namespace

std::vector<std::wstring> shotFolders(const std::wstring& gameDir) {
    std::vector<std::wstring> out = {gameDir + L"\\ScreenShot"};
    wchar_t pictures[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_MYPICTURES, nullptr, 0, pictures)))
        out.push_back(std::wstring(pictures) + L"\\Screenshots");
    return out;
}

std::vector<std::pair<std::string, std::string>> recentShots(const std::vector<std::wstring>& folders, size_t most) {
    struct Found {
        std::wstring path;
        unsigned long long time;
    };
    std::vector<Found> found;
    FILETIME nowFt{};
    GetSystemTimeAsFileTime(&nowFt);
    const auto ticks = [](const FILETIME& t) { return (unsigned long long)t.dwHighDateTime << 32 | t.dwLowDateTime; };
    const unsigned long long now = ticks(nowFt), day = 24ull * 3600 * 10000000ull;
    for (const std::wstring& dir : folders) {
        WIN32_FIND_DATAW d{};
        HANDLE h = FindFirstFileW((dir + L"\\*").c_str(), &d);
        if (h == INVALID_HANDLE_VALUE) continue;
        do {
            if ((d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) || !picture(d.cFileName)) continue;
            const unsigned long long t = ticks(d.ftLastWriteTime);
            if (now - t < day) found.push_back(Found{dir + L"\\" + d.cFileName, t});
        } while (FindNextFileW(h, &d));
        FindClose(h);
    }
    std::sort(found.begin(), found.end(), [](const Found& a, const Found& b) { return a.time > b.time; });
    std::vector<std::pair<std::string, std::string>> out;
    IWICImagingFactory* f = nullptr;
    if (found.empty() || FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                                 IID_IWICImagingFactory, reinterpret_cast<void**>(&f))))
        return out;
    for (const Found& x : found) {
        if (out.size() >= most) break;
        std::string jpeg = jpegOf(f, x.path);
        if (jpeg.empty() || jpeg.size() > 600000) continue;  // the site takes the report under 8 MB
        char name[32];
        std::snprintf(name, sizeof name, "shot%u.jpg", unsigned(out.size() + 1));
        out.emplace_back(name, std::move(jpeg));
    }
    release(f);
    return out;
}

}  // namespace uagfx::app
