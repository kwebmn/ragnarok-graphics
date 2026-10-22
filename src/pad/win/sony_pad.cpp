#include "pad/win/sony_pad.hpp"

#include <setupapi.h>

#include <optional>
#include <utility>
extern "C" {  // MinGW's HID headers declare C functions without saying so
#include <hidsdi.h>
}

#include "pad/win/log.hpp"

namespace uapad {
namespace {

// 2.9: inside ddraw.dll, which imports kernel32, user32, gdi32 and msvcrt only: HID and SetupAPI are taken at run time
// from the system's folder. Without them (a Windows stripped of them) no Sony pad is seen; XInput pads still work.
struct HidApi {
    decltype(&HidD_GetHidGuid) getHidGuid = nullptr;
    decltype(&HidD_GetAttributes) getAttributes = nullptr;
    decltype(&HidD_GetPreparsedData) getPreparsedData = nullptr;
    decltype(&HidD_FreePreparsedData) freePreparsedData = nullptr;
    decltype(&HidD_GetFeature) getFeature = nullptr;
    decltype(&HidP_GetCaps) getCaps = nullptr;
    decltype(&SetupDiGetClassDevsA) getClassDevs = nullptr;
    decltype(&SetupDiEnumDeviceInterfaces) enumDeviceInterfaces = nullptr;
    decltype(&SetupDiGetDeviceInterfaceDetailA) getDeviceInterfaceDetail = nullptr;
    decltype(&SetupDiDestroyDeviceInfoList) destroyDeviceInfoList = nullptr;
    bool ok = false;
};

template <class T>
void take(HMODULE dll, const char* name, T& fn) {
    fn = dll ? reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(dll, name))) : nullptr;
}

HMODULE systemLibrary(const char* name) {
    char dir[MAX_PATH];
    const UINT n = GetSystemDirectoryA(dir, MAX_PATH);
    return n && n < MAX_PATH ? LoadLibraryA((std::string(dir, n) + "\\" + name).c_str()) : nullptr;
}

const HidApi& hidApi() {
    static const HidApi api = [] {
        HidApi a;
        const HMODULE hid = systemLibrary("hid.dll"), setup = systemLibrary("setupapi.dll");
        take(hid, "HidD_GetHidGuid", a.getHidGuid);
        take(hid, "HidD_GetAttributes", a.getAttributes);
        take(hid, "HidD_GetPreparsedData", a.getPreparsedData);
        take(hid, "HidD_FreePreparsedData", a.freePreparsedData);
        take(hid, "HidD_GetFeature", a.getFeature);
        take(hid, "HidP_GetCaps", a.getCaps);
        take(setup, "SetupDiGetClassDevsA", a.getClassDevs);
        take(setup, "SetupDiEnumDeviceInterfaces", a.enumDeviceInterfaces);
        take(setup, "SetupDiGetDeviceInterfaceDetailA", a.getDeviceInterfaceDetail);
        take(setup, "SetupDiDestroyDeviceInfoList", a.destroyDeviceInfoList);
        a.ok = a.getHidGuid && a.getAttributes && a.getPreparsedData && a.freePreparsedData && a.getFeature &&
               a.getCaps && a.getClassDevs && a.enumDeviceInterfaces && a.getDeviceInterfaceDetail &&
               a.destroyDeviceInfoList;
        if (!a.ok) LOG_INFO("HID or SetupAPI not found: no PlayStation pad, XInput pads only");
        return a;
    }();
    return api;
}

// 2.9: the HID devices Windows lists for raw input (vendor, product), read from user32's own list with no device
// opened; nothing when the list cannot be read. With the pad on for everyone, the scan below (every HID device opened,
// once a second while no controller is found) waits for a Sony one among them.
std::optional<std::vector<std::pair<uint16_t, uint16_t>>> listedHid() {
    UINT n = 0;
    if (GetRawInputDeviceList(nullptr, &n, sizeof(RAWINPUTDEVICELIST)) == UINT(-1) || n == 0) return std::nullopt;
    std::vector<RAWINPUTDEVICELIST> list(n + 8);  // room for a device that comes meanwhile
    n = UINT(list.size());
    const UINT got = GetRawInputDeviceList(list.data(), &n, sizeof(RAWINPUTDEVICELIST));
    if (got == UINT(-1)) return std::nullopt;
    std::vector<std::pair<uint16_t, uint16_t>> ids;
    for (UINT i = 0; i < got; ++i) {
        if (list[i].dwType != RIM_TYPEHID) continue;
        RID_DEVICE_INFO info{};
        info.cbSize = sizeof info;
        UINT size = sizeof info;
        if (GetRawInputDeviceInfoA(list[i].hDevice, RIDI_DEVICEINFO, &info, &size) == UINT(-1)) continue;
        ids.emplace_back(uint16_t(info.hid.dwVendorId), uint16_t(info.hid.dwProductId));
    }
    return ids;
}

// A HID device's path whose vendor and product are a Sony controller's - the first found - and its input report's
// length; how it is connected from its path (Bluetooth's go through the BTHENUM or a {00001124-...} service).
struct Found {
    std::string path;
    sony::Model model = sony::Model::None;
    USHORT inputLength = 64;
};

Found findController() {
    Found f;
    const HidApi& api = hidApi();
    if (!api.ok) return f;
    GUID hid;
    api.getHidGuid(&hid);
    HDEVINFO set = api.getClassDevs(&hid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return f;
    SP_DEVICE_INTERFACE_DATA iface{};
    iface.cbSize = sizeof iface;
    for (DWORD i = 0; f.model == sony::Model::None && api.enumDeviceInterfaces(set, nullptr, &hid, i, &iface); ++i) {
        DWORD size = 0;
        api.getDeviceInterfaceDetail(set, &iface, nullptr, 0, &size, nullptr);
        if (!size) continue;
        std::vector<uint8_t> buf(size);
        auto* detail = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_A*>(buf.data());
        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
        if (!api.getDeviceInterfaceDetail(set, &iface, detail, size, nullptr, nullptr)) continue;
        HANDLE h = CreateFileA(detail->DevicePath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0,
                               nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;
        HIDD_ATTRIBUTES a{};
        a.Size = sizeof a;
        if (api.getAttributes(h, &a)) {
            const sony::Model m = sony::modelOf(a.VendorID, a.ProductID);
            PHIDP_PREPARSED_DATA pre = nullptr;
            HIDP_CAPS caps{};
            // A controller shows up as several collections: the gamepad's has input reports of 10 bytes or more.
            if (m != sony::Model::None && api.getPreparsedData(h, &pre)) {
                if (api.getCaps(pre, &caps) == HIDP_STATUS_SUCCESS && caps.InputReportByteLength >= 10) {
                    f.path = detail->DevicePath;
                    f.model = m;
                    f.inputLength = caps.InputReportByteLength;
                }
                api.freePreparsedData(pre);
            }
        }
        CloseHandle(h);
    }
    api.destroyDeviceInfoList(set);
    return f;
}

}  // namespace

SonyPad::~SonyPad() { close(); }

const char* SonyPad::name() const {
    if (model_ == sony::Model::DualShock4) return bluetooth_ ? "DualShock 4 (Bluetooth)" : "DualShock 4 (USB)";
    if (model_ == sony::Model::DualSense) return bluetooth_ ? "DualSense (Bluetooth)" : "DualSense (USB)";
    return "none";
}

void SonyPad::close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CancelIo(handle_);
        CloseHandle(handle_);
    }
    if (event_) CloseHandle(event_);
    handle_ = INVALID_HANDLE_VALUE;
    event_ = nullptr;
    reading_ = false;
    model_ = sony::Model::None;
    last_ = PadState{};
}

bool SonyPad::find() {
    const Found f = findController();
    if (f.model == sony::Model::None) return false;
    handle_ = CreateFileA(f.path.c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                          OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) return false;
    event_ = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    model_ = f.model;
    // Bluetooth: its reports are longer than USB's (78 against 64), or its path says so.
    bluetooth_ = f.inputLength > 64 || f.path.find("{00001124-") != std::string::npos ||
                 f.path.find("bthenum") != std::string::npos || f.path.find("BTHENUM") != std::string::npos;
    buffer_.assign(f.inputLength, 0);
    // Over Bluetooth a feature report asks it for its full input reports (the DualShock 4's 0x02, the DualSense's
    // 0x05).
    if (bluetooth_) {
        uint8_t feature[64] = {uint8_t(model_ == sony::Model::DualShock4 ? 0x02 : 0x05)};
        if (hidApi().ok) hidApi().getFeature(handle_, feature, model_ == sony::Model::DualShock4 ? 37 : 41);
    }
    LOG_INFO("pad: %s found", name());
    send(0, 0);  // its light bar's colour
    return true;
}

void SonyPad::startRead() {
    if (reading_ || handle_ == INVALID_HANDLE_VALUE) return;
    ResetEvent(event_);
    overlapped_ = OVERLAPPED{};
    overlapped_.hEvent = event_;
    DWORD got = 0;
    if (ReadFile(handle_, buffer_.data(), DWORD(buffer_.size()), &got, &overlapped_) ||
        GetLastError() == ERROR_IO_PENDING) {
        reading_ = true;
        return;
    }
    LOG_INFO("pad: %s gone", name());
    close();
}

PadState SonyPad::poll() {
    if (handle_ == INVALID_HANDLE_VALUE) {
        const DWORD now = GetTickCount();
        if (scanned_ && now - lastScanMs_ < 1000) return PadState{};
        scanned_ = true;
        lastScanMs_ = now;
        const auto listed = listedHid();  // the full scan when a Sony controller is listed, or the list unread
        if ((listed && !sony::anyController(*listed)) || !find()) return PadState{};
    }
    // Every report that came since the last poll, the newest kept.
    for (int k = 0; k < 16; ++k) {
        startRead();
        if (!reading_) return PadState{};
        DWORD got = 0;
        if (!GetOverlappedResult(handle_, &overlapped_, &got, FALSE)) {
            if (GetLastError() == ERROR_IO_INCOMPLETE) break;  // none more yet
            LOG_INFO("pad: %s gone", name());
            close();
            return PadState{};
        }
        reading_ = false;
        PadState p;
        if (sony::parse(model_, buffer_.data(), got, touchpad_, p)) last_ = p;
    }
    return last_;
}

void SonyPad::send(uint8_t left, uint8_t right) {
    if (handle_ == INVALID_HANDLE_VALUE) return;
    const std::vector<uint8_t> o = sony::output(model_, bluetooth_, left, right, lightbar_, sequence_++);
    if (o.empty()) return;
    OVERLAPPED w{};
    w.hEvent = CreateEventA(nullptr, TRUE, FALSE, nullptr);
    DWORD wrote = 0;
    if (!WriteFile(handle_, o.data(), DWORD(o.size()), &wrote, &w) && GetLastError() == ERROR_IO_PENDING)
        WaitForSingleObject(w.hEvent, 20);  // a report is small: done at once, or let go
    CancelIoEx(handle_, &w);
    CloseHandle(w.hEvent);
}

void SonyPad::rumble(Rumble pattern, DWORD nowMs) {
    pattern_.clear();
    switch (pattern) {  // as XInputPad's, the motors 0-255
        case Rumble::TargetSwitch: pattern_ = {{80, 0, 0x60}}; break;
        case Rumble::TargetCleared: pattern_ = {{60, 0x30, 0}}; break;
        case Rumble::PadOn: pattern_ = {{60, 0x50, 0x50}, {60, 0, 0}, {60, 0x50, 0x50}}; break;
        case Rumble::PadOff: pattern_ = {{250, 0x80, 0}}; break;
        case Rumble::Denied: pattern_ = {{40, 0, 0x30}}; break;
        case Rumble::CameraOn: pattern_ = {{40, 0x20, 0x60}, {50, 0, 0}, {40, 0x20, 0x60}}; break;
        case Rumble::CameraOff: pattern_ = {{90, 0x60, 0x20}}; break;
        case Rumble::None: break;
    }
    seg_ = 0;
    segStart_ = nowMs;
    if (!pattern_.empty()) send(pattern_[0].left, pattern_[0].right);
}

void SonyPad::update(DWORD nowMs) {
    if (pattern_.empty()) return;
    while (seg_ < pattern_.size() && nowMs - segStart_ >= pattern_[seg_].ms) {
        segStart_ += pattern_[seg_].ms;
        ++seg_;
        if (seg_ < pattern_.size()) send(pattern_[seg_].left, pattern_[seg_].right);
    }
    if (seg_ >= pattern_.size()) stopRumble();
}

void SonyPad::stopRumble() {
    if (pattern_.empty()) return;
    pattern_.clear();
    send(0, 0);
}

}  // namespace uapad
