#include "pad/win/hooks.hpp"

#include <algorithm>
#include <vector>

#include "pad/core/call_targets.hpp"
#include "pad/win/iat.hpp"
#include "pad/win/log.hpp"
#include "pad/win/runtime.hpp"

namespace uapad::hooks {
namespace {

using recv_t = int(WSAAPI*)(SOCKET, char*, int, int);
using send_t = int(WSAAPI*)(SOCKET, const char*, int, int);
using closesocket_t = int(WSAAPI*)(SOCKET);
using getcursorpos_t = BOOL(WINAPI*)(LPPOINT);
using peekmessage_t = BOOL(WINAPI*)(LPMSG, HWND, UINT, UINT, UINT);
using wsagetlasterror_t = int(WSAAPI*)();

recv_t o_recv = nullptr;
send_t o_send = nullptr;
std::vector<uint32_t> g_pointerSlots;  // client variables called as `call [X]`
closesocket_t o_closesocket = nullptr;
getcursorpos_t o_getcursorpos = nullptr;
peekmessage_t o_peekmessage = nullptr;
wsagetlasterror_t o_wsagetlasterror = nullptr;

int WSAAPI h_recv(SOCKET s, char* buf, int len, int flags) {
    int n = o_recv(s, buf, len, flags);
    if (n > 0 && !(flags & MSG_PEEK)) {
        runtime::onRecv(s, buf, n);
        n += runtime::extendRecv(s, buf, n, len);
    }
    return n;
}

int WSAAPI h_send(SOCKET s, const char* buf, int len, int flags) {
    const int n = o_send(s, buf, len, flags);
    if (n > 0) runtime::onSent(s, buf, n);
    return n;
}

int WSAAPI h_closesocket(SOCKET s) {
    runtime::onClose(s);
    return o_closesocket(s);
}

BOOL WINAPI h_getcursorpos(LPPOINT p) {
    const BOOL ok = o_getcursorpos(p);
    if (ok) runtime::onGetCursorPos(p);
    return ok;
}

BOOL WINAPI h_peekmessage(LPMSG msg, HWND hwnd, UINT first, UINT last, UINT remove) {
    runtime::onMessagePump();
    const BOOL got = o_peekmessage(msg, hwnd, first, last, remove);
    if (!got) runtime::onQueueEmpty();
    return got;
}

template <class T>
T proc(HMODULE dll, const char* name) {
    return reinterpret_cast<T>(reinterpret_cast<void*>(GetProcAddress(dll, name)));
}

template <class T>
void hook(HMODULE dll, const char* name, T replacement, T& original) {
    T target = proc<T>(dll, name);
    original = target;
    if (!target) {
        LOG_ERROR("hook %s: export not found", name);
        return;
    }
    if (iat::patch(GetModuleHandleA(nullptr), reinterpret_cast<void*>(target), reinterpret_cast<void*>(replacement)))
        LOG_INFO("hooked %s", name);
    else
        LOG_ERROR("hook %s: not in the client's import table", name);
}

}  // namespace

void install() {
    HMODULE ws2 = GetModuleHandleA("ws2_32.dll");
    if (!ws2) ws2 = LoadLibraryA("ws2_32.dll");
    HMODULE user32 = GetModuleHandleA("user32.dll");
    o_wsagetlasterror = proc<wsagetlasterror_t>(ws2, "WSAGetLastError");
    hook(ws2, "recv", &h_recv, o_recv);
    // send stays untouched in the import table: the client's protection thread compares that slot (and
    // WSASend's) with what it saw at start-up. Game traffic goes through the client's own pointers anyway.
    o_send = proc<send_t>(ws2, "send");
    hook(ws2, "closesocket", &h_closesocket, o_closesocket);
    hook(user32, "GetCursorPos", &h_getcursorpos, o_getcursorpos);
    hook(user32, "PeekMessageA", &h_peekmessage, o_peekmessage);
    findClientPointers();
}

void findClientPointers() {
    auto* base = reinterpret_cast<BYTE*>(GetModuleHandleA(nullptr));
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
    const IMAGE_SECTION_HEADER* first = IMAGE_FIRST_SECTION(nt);
    const WORD count = nt->FileHeader.NumberOfSections;
    g_pointerSlots.clear();
    for (WORD c = 0; c < count; ++c) {
        if (!(first[c].Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        const uint8_t* code = base + first[c].VirtualAddress;
        const size_t codeLen = first[c].Misc.VirtualSize;
        for (WORD d = 0; d < count; ++d) {
            if (!(first[d].Characteristics & IMAGE_SCN_MEM_WRITE) || (first[d].Characteristics & IMAGE_SCN_MEM_EXECUTE))
                continue;
            const uint32_t lo = uint32_t(reinterpret_cast<uintptr_t>(base) + first[d].VirtualAddress);
            const uint32_t hi = lo + std::max<uint32_t>(first[d].Misc.VirtualSize, first[d].SizeOfRawData);
            for (uint32_t x : indirectCallTargets(code, codeLen, lo, hi)) g_pointerSlots.push_back(x);
        }
    }
    LOG_INFO("client call-through pointers: %u", unsigned(g_pointerSlots.size()));
}

int patchClientPointers() {
    void* const realSend = reinterpret_cast<void*>(o_send);
    void* const realRecv = reinterpret_cast<void*>(o_recv);
    void* const ourSend = reinterpret_cast<void*>(&h_send);
    void* const ourRecv = reinterpret_cast<void*>(&h_recv);
    int ours = 0;
    for (uint32_t slot : g_pointerSlots) {
        auto* p = reinterpret_cast<void* volatile*>(uintptr_t(slot));
        void* const v = *p;
        if (v == ourSend || v == ourRecv) {
            ++ours;
            continue;
        }
        void* const with = v == realSend && realSend ? ourSend : v == realRecv && realRecv ? ourRecv : nullptr;
        if (!with) continue;
        *p = with;  // a plain .data variable of the client; the game thread is the only caller
        ++ours;
        LOG_INFO("client %s pointer at 0x%08lx now goes through the pad", with == ourSend ? "send" : "recv",
                 (unsigned long)slot);
    }
    return ours;
}

int lastError() { return o_wsagetlasterror ? o_wsagetlasterror() : 0; }

bool sendAll(SOCKET s, const uint8_t* data, int len) {
    if (!o_send) return false;
    int done = 0;
    int retries = 0;
    while (done < len) {
        const int n = o_send(s, reinterpret_cast<const char*>(data) + done, len - done, 0);
        if (n > 0) {
            done += n;
            continue;
        }
        if (n == SOCKET_ERROR && lastError() == WSAEWOULDBLOCK && ++retries <= 50) {
            Sleep(1);
            continue;
        }
        return false;
    }
    return true;
}

}  // namespace uapad::hooks
