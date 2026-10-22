#define INITGUID
#include "pad/win/di_wrappers.hpp"

#include "pad/win/log.hpp"
#include "pad/win/runtime.hpp"

namespace uapad {
namespace {

enum class DevKind { Mouse, Keyboard };

// Forwards everything to the real device; the mouse also reports every poll to the runtime.
class DeviceWrap final : public IDirectInputDevice7A {
public:
    DeviceWrap(IDirectInputDeviceA* real, DevKind kind) : real_(real), kind_(kind) {}

    STDMETHODIMP QueryInterface(REFIID riid, LPVOID* ppv) override {
        if (!ppv) return E_POINTER;
        const bool base = IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IDirectInputDeviceA);
        const bool newer = IsEqualGUID(riid, IID_IDirectInputDevice2A) || IsEqualGUID(riid, IID_IDirectInputDevice7A);
        if (base || (newer && ensure(riid))) {
            AddRef();
            *ppv = static_cast<IDirectInputDevice7A*>(this);
            return S_OK;
        }
        return real_->QueryInterface(riid, ppv);
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ULONG(InterlockedIncrement(&refs_)); }
    STDMETHODIMP_(ULONG) Release() override {
        const LONG n = InterlockedDecrement(&refs_);
        if (n == 0) {
            if (v7_) v7_->Release();
            if (v2_) v2_->Release();
            real_->Release();
            delete this;
        }
        return ULONG(n);
    }

    // IDirectInputDeviceA
    STDMETHODIMP GetCapabilities(LPDIDEVCAPS caps) override { return real_->GetCapabilities(caps); }
    STDMETHODIMP EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA cb, LPVOID ref, DWORD flags) override {
        return real_->EnumObjects(cb, ref, flags);
    }
    STDMETHODIMP GetProperty(REFGUID prop, LPDIPROPHEADER hdr) override { return real_->GetProperty(prop, hdr); }
    STDMETHODIMP SetProperty(REFGUID prop, LPCDIPROPHEADER hdr) override {
        if (&prop == &DIPROP_BUFFERSIZE && hdr)
            LOG_INFO("%s buffer size %lu", name(), (unsigned long)reinterpret_cast<LPCDIPROPDWORD>(hdr)->dwData);
        return real_->SetProperty(prop, hdr);
    }
    STDMETHODIMP Acquire() override { return real_->Acquire(); }
    STDMETHODIMP Unacquire() override { return real_->Unacquire(); }
    STDMETHODIMP GetDeviceState(DWORD cb, LPVOID data) override {
        const HRESULT hr = real_->GetDeviceState(cb, data);
        if (kind_ == DevKind::Mouse && SUCCEEDED(hr)) runtime::onMouseState(cb, data);
        return hr;
    }
    STDMETHODIMP GetDeviceData(DWORD cb, LPDIDEVICEOBJECTDATA rgdod, LPDWORD inOut, DWORD flags) override {
        const DWORD capacity = inOut ? *inOut : 0;
        const HRESULT hr = real_->GetDeviceData(cb, rgdod, inOut, flags);
        if (kind_ == DevKind::Mouse && SUCCEEDED(hr)) runtime::onMouseData(cb, rgdod, inOut, capacity, flags);
        return hr;
    }
    STDMETHODIMP SetDataFormat(LPCDIDATAFORMAT fmt) override {
        if (fmt) LOG_INFO("%s data format: %lu bytes", name(), (unsigned long)fmt->dwDataSize);
        return real_->SetDataFormat(fmt);
    }
    STDMETHODIMP SetEventNotification(HANDLE ev) override { return real_->SetEventNotification(ev); }
    STDMETHODIMP SetCooperativeLevel(HWND hwnd, DWORD flags) override {
        LOG_INFO("%s cooperative level 0x%lx", name(), (unsigned long)flags);
        if (kind_ == DevKind::Mouse) {
            runtime::setGameWindow(hwnd);
            flags = runtime::mouseCooperativeLevel(flags);
        }
        return real_->SetCooperativeLevel(hwnd, flags);
    }
    STDMETHODIMP GetObjectInfo(LPDIDEVICEOBJECTINSTANCEA info, DWORD obj, DWORD how) override {
        return real_->GetObjectInfo(info, obj, how);
    }
    STDMETHODIMP GetDeviceInfo(LPDIDEVICEINSTANCEA info) override { return real_->GetDeviceInfo(info); }
    STDMETHODIMP RunControlPanel(HWND owner, DWORD flags) override { return real_->RunControlPanel(owner, flags); }
    STDMETHODIMP Initialize(HINSTANCE inst, DWORD version, REFGUID guid) override {
        return real_->Initialize(inst, version, guid);
    }

    // IDirectInputDevice2A
    STDMETHODIMP CreateEffect(REFGUID g, LPCDIEFFECT e, LPDIRECTINPUTEFFECT* out, LPUNKNOWN outer) override {
        return ensure(IID_IDirectInputDevice2A) ? v2_->CreateEffect(g, e, out, outer) : E_NOTIMPL;
    }
    STDMETHODIMP EnumEffects(LPDIENUMEFFECTSCALLBACKA cb, LPVOID ref, DWORD type) override {
        return ensure(IID_IDirectInputDevice2A) ? v2_->EnumEffects(cb, ref, type) : E_NOTIMPL;
    }
    STDMETHODIMP GetEffectInfo(LPDIEFFECTINFOA info, REFGUID g) override {
        return ensure(IID_IDirectInputDevice2A) ? v2_->GetEffectInfo(info, g) : E_NOTIMPL;
    }
    STDMETHODIMP GetForceFeedbackState(LPDWORD out) override {
        return ensure(IID_IDirectInputDevice2A) ? v2_->GetForceFeedbackState(out) : E_NOTIMPL;
    }
    STDMETHODIMP SendForceFeedbackCommand(DWORD cmd) override {
        return ensure(IID_IDirectInputDevice2A) ? v2_->SendForceFeedbackCommand(cmd) : E_NOTIMPL;
    }
    STDMETHODIMP EnumCreatedEffectObjects(LPDIENUMCREATEDEFFECTOBJECTSCALLBACK cb, LPVOID ref, DWORD flags) override {
        return ensure(IID_IDirectInputDevice2A) ? v2_->EnumCreatedEffectObjects(cb, ref, flags) : E_NOTIMPL;
    }
    STDMETHODIMP Escape(LPDIEFFESCAPE esc) override {
        return ensure(IID_IDirectInputDevice2A) ? v2_->Escape(esc) : E_NOTIMPL;
    }
    STDMETHODIMP Poll() override { return ensure(IID_IDirectInputDevice2A) ? v2_->Poll() : E_NOTIMPL; }
    STDMETHODIMP SendDeviceData(DWORD cb, LPCDIDEVICEOBJECTDATA data, LPDWORD inOut, DWORD flags) override {
        return ensure(IID_IDirectInputDevice2A) ? v2_->SendDeviceData(cb, data, inOut, flags) : E_NOTIMPL;
    }

    // IDirectInputDevice7A
    STDMETHODIMP EnumEffectsInFile(LPCSTR file, LPDIENUMEFFECTSINFILECALLBACK cb, LPVOID ref, DWORD flags) override {
        return ensure(IID_IDirectInputDevice7A) ? v7_->EnumEffectsInFile(file, cb, ref, flags) : E_NOTIMPL;
    }
    STDMETHODIMP WriteEffectToFile(LPCSTR file, DWORD count, LPDIFILEEFFECT effects, DWORD flags) override {
        return ensure(IID_IDirectInputDevice7A) ? v7_->WriteEffectToFile(file, count, effects, flags) : E_NOTIMPL;
    }

private:
    bool ensure(REFIID riid) {
        if (IsEqualGUID(riid, IID_IDirectInputDevice7A)) {
            if (!v7_ && FAILED(real_->QueryInterface(IID_IDirectInputDevice7A, reinterpret_cast<void**>(&v7_))))
                v7_ = nullptr;
            return v7_ != nullptr;
        }
        if (!v2_ && FAILED(real_->QueryInterface(IID_IDirectInputDevice2A, reinterpret_cast<void**>(&v2_))))
            v2_ = nullptr;
        return v2_ != nullptr;
    }
    const char* name() const { return kind_ == DevKind::Mouse ? "mouse" : "keyboard"; }

    IDirectInputDeviceA* real_;
    IDirectInputDevice2A* v2_ = nullptr;
    IDirectInputDevice7A* v7_ = nullptr;
    DevKind kind_;
    LONG refs_ = 1;
};

IDirectInputDeviceA* wrapDevice(REFGUID guid, IDirectInputDeviceA* dev) {
    if (IsEqualGUID(guid, GUID_SysMouse)) {
        LOG_INFO("mouse device created");
        return new DeviceWrap(dev, DevKind::Mouse);
    }
    if (IsEqualGUID(guid, GUID_SysKeyboard)) {
        LOG_INFO("keyboard device created");
        return new DeviceWrap(dev, DevKind::Keyboard);
    }
    return dev;
}

class InputWrap final : public IDirectInput7A {
public:
    explicit InputWrap(IDirectInputA* real) : real_(real) {}

    STDMETHODIMP QueryInterface(REFIID riid, LPVOID* ppv) override {
        if (!ppv) return E_POINTER;
        const bool base = IsEqualGUID(riid, IID_IUnknown) || IsEqualGUID(riid, IID_IDirectInputA);
        const bool newer = IsEqualGUID(riid, IID_IDirectInput2A) || IsEqualGUID(riid, IID_IDirectInput7A);
        if (base || (newer && ensure(riid))) {
            AddRef();
            *ppv = static_cast<IDirectInput7A*>(this);
            return S_OK;
        }
        return real_->QueryInterface(riid, ppv);
    }
    STDMETHODIMP_(ULONG) AddRef() override { return ULONG(InterlockedIncrement(&refs_)); }
    STDMETHODIMP_(ULONG) Release() override {
        const LONG n = InterlockedDecrement(&refs_);
        if (n == 0) {
            if (v7_) v7_->Release();
            if (v2_) v2_->Release();
            real_->Release();
            delete this;
        }
        return ULONG(n);
    }
    STDMETHODIMP CreateDevice(REFGUID guid, LPDIRECTINPUTDEVICEA* out, LPUNKNOWN outer) override {
        const HRESULT hr = real_->CreateDevice(guid, out, outer);
        if (SUCCEEDED(hr) && out && *out && !outer) *out = wrapDevice(guid, *out);
        return hr;
    }
    STDMETHODIMP EnumDevices(DWORD type, LPDIENUMDEVICESCALLBACKA cb, LPVOID ref, DWORD flags) override {
        return real_->EnumDevices(type, cb, ref, flags);
    }
    STDMETHODIMP GetDeviceStatus(REFGUID guid) override { return real_->GetDeviceStatus(guid); }
    STDMETHODIMP RunControlPanel(HWND owner, DWORD flags) override { return real_->RunControlPanel(owner, flags); }
    STDMETHODIMP Initialize(HINSTANCE inst, DWORD version) override { return real_->Initialize(inst, version); }
    STDMETHODIMP FindDevice(REFGUID guid, LPCSTR name, LPGUID out) override {
        return ensure(IID_IDirectInput2A) ? v2_->FindDevice(guid, name, out) : E_NOTIMPL;
    }
    STDMETHODIMP CreateDeviceEx(REFGUID guid, REFIID riid, LPVOID* out, LPUNKNOWN outer) override {
        if (!ensure(IID_IDirectInput7A)) return E_NOTIMPL;
        if (outer) return v7_->CreateDeviceEx(guid, riid, out, outer);
        IDirectInputDeviceA* dev = nullptr;
        HRESULT hr = v7_->CreateDeviceEx(guid, IID_IDirectInputDeviceA, reinterpret_cast<LPVOID*>(&dev), nullptr);
        if (FAILED(hr) || !dev) return hr;
        IDirectInputDeviceA* wrapped = wrapDevice(guid, dev);
        hr = wrapped->QueryInterface(riid, out);
        wrapped->Release();
        return hr;
    }

private:
    bool ensure(REFIID riid) {
        if (IsEqualGUID(riid, IID_IDirectInput7A)) {
            if (!v7_ && FAILED(real_->QueryInterface(IID_IDirectInput7A, reinterpret_cast<void**>(&v7_))))
                v7_ = nullptr;
            return v7_ != nullptr;
        }
        if (!v2_ && FAILED(real_->QueryInterface(IID_IDirectInput2A, reinterpret_cast<void**>(&v2_)))) v2_ = nullptr;
        return v2_ != nullptr;
    }

    IDirectInputA* real_;
    IDirectInput2A* v2_ = nullptr;
    IDirectInput7A* v7_ = nullptr;
    LONG refs_ = 1;
};

}  // namespace

IDirectInputA* wrapDirectInput(IDirectInputA* real) { return new InputWrap(real); }

}  // namespace uapad
