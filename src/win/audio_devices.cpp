#include "win/audio_devices.hpp"

#include <windows.h>
#include <mmdeviceapi.h>

namespace uagfx::win::audio_devices {
namespace {

using CoInitFn = HRESULT(WINAPI*)(void*, DWORD);
using CoUninitFn = void(WINAPI*)();
using CoCreateFn = HRESULT(WINAPI*)(REFCLSID, IUnknown*, DWORD, REFIID, void**);
using CoFreeFn = void(WINAPI*)(void*);
using ClearFn = HRESULT(WINAPI*)(PROPVARIANT*);

struct Ole {
    CoInitFn init = nullptr;
    CoUninitFn uninit = nullptr;
    CoCreateFn create = nullptr;
    CoFreeFn free = nullptr;
    ClearFn clear = nullptr;
};

template <typename Fn>
Fn proc(HMODULE m, const char* name) {
    return m ? reinterpret_cast<Fn>(reinterpret_cast<void*>(GetProcAddress(m, name))) : nullptr;
}

const Ole* ole() {
    static const Ole o = [] {
        const HMODULE m = LoadLibraryA("ole32.dll");
        return Ole{proc<CoInitFn>(m, "CoInitializeEx"), proc<CoUninitFn>(m, "CoUninitialize"),
                   proc<CoCreateFn>(m, "CoCreateInstance"), proc<CoFreeFn>(m, "CoTaskMemFree"),
                   proc<ClearFn>(m, "PropVariantClear")};
    }();
    return o.init && o.uninit && o.create && o.free && o.clear ? &o : nullptr;
}

// PKEY_Device_FriendlyName (functiondiscoverykeys_devpkey.h): "Навушники (WH-1000XM4)"
const PROPERTYKEY kFriendlyName = {{0xa45c254e, 0xdf1c, 0x4efd, {0x80, 0x20, 0x67, 0xd1, 0x46, 0xa8, 0x50, 0xe0}}, 14};

std::string narrow(const wchar_t* w) {
    if (!w || !*w) return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(size_t(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, &s[0], n, nullptr, nullptr);
    return s;
}

std::string idOf(IMMDevice& d) {
    LPWSTR w = nullptr;
    if (FAILED(d.GetId(&w)) || !w) return {};
    std::string s = narrow(w);
    ole()->free(w);
    return s;
}

std::string nameOfDevice(IMMDevice& d) {
    IPropertyStore* store = nullptr;
    if (FAILED(d.OpenPropertyStore(STGM_READ, &store)) || !store) return {};
    PROPVARIANT v;
    PropVariantInit(&v);
    std::string name;
    if (SUCCEEDED(store->GetValue(kFriendlyName, &v)) && v.vt == VT_LPWSTR) name = narrow(v.pwszVal);
    ole()->clear(&v);
    store->Release();
    return name;
}

bool enumerate(IMMDeviceEnumerator& e, std::vector<Device>& active, std::string& defaultId) {
    IMMDeviceCollection* all = nullptr;
    if (FAILED(e.EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &all)) || !all) return false;
    UINT n = 0;
    if (FAILED(all->GetCount(&n))) n = 0;
    for (UINT i = 0; i < n; ++i) {
        IMMDevice* d = nullptr;
        if (FAILED(all->Item(i, &d)) || !d) continue;
        Device device{idOf(*d), nameOfDevice(*d)};
        if (!device.id.empty()) active.push_back(device);
        d->Release();
    }
    all->Release();
    IMMDevice* d = nullptr;  // none when there is no output device at all
    if (SUCCEEDED(e.GetDefaultAudioEndpoint(eRender, eConsole, &d)) && d) {
        defaultId = idOf(*d);
        d->Release();
    }
    return true;
}

// A question for Windows: its own thread (COM in the multithreaded apartment); left to that thread after 5 s.
struct Job {
    volatile LONG refs = 2;
    bool names = false;  // nameOf's: `id`'s name; else the list
    std::string id, name, defaultId;
    std::vector<Device> active;
    bool ok = false;
};

void drop(Job* j) {
    if (InterlockedDecrement(&j->refs) == 0) delete j;
}

DWORD WINAPI work(void* p) {
    Job* j = static_cast<Job*>(p);
    const Ole* o = ole();
    const HRESULT init = o->init(nullptr, COINIT_MULTITHREADED);
    IMMDeviceEnumerator* e = nullptr;
    if (SUCCEEDED(o->create(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                            reinterpret_cast<void**>(&e))) &&
        e) {
        if (j->names) {
            IMMDevice* d = nullptr;
            const std::wstring id = wide(j->id);
            if (SUCCEEDED(e->GetDevice(id.c_str(), &d)) && d) {
                j->name = nameOfDevice(*d);
                d->Release();
            }
            j->ok = true;
        } else {
            j->ok = enumerate(*e, j->active, j->defaultId);
        }
        e->Release();
    }
    if (SUCCEEDED(init)) o->uninit();
    drop(j);
    return 0;
}

// The job done (the caller's to drop), or nullptr
Job* ask(Job* j) {
    if (!ole()) {
        delete j;
        return nullptr;
    }
    const HANDLE t = CreateThread(nullptr, 0, work, j, 0, nullptr);
    if (!t) {
        delete j;
        return nullptr;
    }
    const bool done = WaitForSingleObject(t, 5000) == WAIT_OBJECT_0;
    CloseHandle(t);
    if (done) return j;
    drop(j);
    return nullptr;
}

// Windows' notices of output devices: counted, nothing more (its own thread calls them)
class Notices : public IMMNotificationClient {
public:
    volatile LONG count = 0;
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override {
        if (!out) return E_POINTER;
        if (IsEqualIID(iid, __uuidof(IUnknown)) || IsEqualIID(iid, __uuidof(IMMNotificationClient))) {
            *out = static_cast<IMMNotificationClient*>(this);
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }
    ULONG STDMETHODCALLTYPE AddRef() override { return 1; }
    ULONG STDMETHODCALLTYPE Release() override { return 1; }
    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR, DWORD) override { return bump(); }
    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR) override { return bump(); }
    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR) override { return bump(); }
    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole, LPCWSTR) override {
        return flow == eRender ? bump() : S_OK;
    }
    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    HRESULT bump() {
        InterlockedIncrement(&count);
        return S_OK;
    }
};

Notices g_notices;

// The watching thread: registered, then asleep for good (the notices come on Windows' own thread)
DWORD WINAPI watch(void*) {
    const Ole* o = ole();
    if (!o || FAILED(o->init(nullptr, COINIT_MULTITHREADED))) return 0;
    IMMDeviceEnumerator* e = nullptr;
    if (FAILED(o->create(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                         reinterpret_cast<void**>(&e))) ||
        !e || FAILED(e->RegisterEndpointNotificationCallback(&g_notices))) {
        if (e) e->Release();
        o->uninit();
        return 0;
    }
    Sleep(INFINITE);
    return 0;
}

}  // namespace

std::wstring wide(const std::string& id) {
    if (id.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, id.c_str(), -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring w(size_t(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, id.c_str(), -1, &w[0], n);
    return w;
}

bool list(std::vector<Device>& active, std::string& defaultId) {
    Job* j = ask(new Job);
    if (!j) return false;
    const bool ok = j->ok;
    if (ok) {
        active = j->active;
        defaultId = j->defaultId;
    }
    drop(j);
    return ok;
}

std::string nameOf(const std::string& id) {
    if (id.empty()) return {};
    Job* asked = new Job;
    asked->names = true;
    asked->id = id;
    Job* j = ask(asked);
    if (!j) return {};
    std::string name = j->name;
    drop(j);
    return name;
}

unsigned changes() {
    static const bool started = [] {
        const HANDLE t = ole() ? CreateThread(nullptr, 0, watch, nullptr, 0, nullptr) : nullptr;
        if (t) CloseHandle(t);
        return t != nullptr;
    }();
    (void)started;
    return unsigned(g_notices.count);
}

}  // namespace uagfx::win::audio_devices
