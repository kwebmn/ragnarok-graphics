#include "win/safe_read.hpp"

#include <windows.h>

#include "win/log.hpp"

// The copy: rep movsb between two labels. A fault comes with EIP at its rep movsb (a string instruction faults before
// its step, ECX/ESI/EDI telling how far it got); the handler then moves EIP to the fault's way out, which pops what
// the entry pushed and answers 0. The names are given whole (asm labels): no underscore of the C names in between.
extern "C" int uagfx_safe_copy(void* out, const void* from, size_t size) asm("uagfx_safe_copy");
extern "C" const char uagfx_safe_copy_at[] asm("uagfx_safe_copy_at");
extern "C" const char uagfx_safe_copy_fault[] asm("uagfx_safe_copy_fault");

asm(".text\n"
    ".p2align 4\n"
    ".globl uagfx_safe_copy\n"
    "uagfx_safe_copy:\n"
    "    pushl %esi\n"
    "    pushl %edi\n"
    "    movl 12(%esp), %edi\n"  // out (the return address and the two pushes below it)
    "    movl 16(%esp), %esi\n"  // from
    "    movl 20(%esp), %ecx\n"  // size
    ".globl uagfx_safe_copy_at\n"
    "uagfx_safe_copy_at:\n"
    "    rep movsb\n"
    "    popl %edi\n"
    "    popl %esi\n"
    "    movl $1, %eax\n"
    "    ret\n"
    ".globl uagfx_safe_copy_fault\n"
    "uagfx_safe_copy_fault:\n"
    "    popl %edi\n"
    "    popl %esi\n"
    "    xorl %eax, %eax\n"
    "    ret\n");

namespace uagfx::win::saferead {
namespace {

volatile LONG g_faults = 0;

LONG CALLBACK onCopyFault(EXCEPTION_POINTERS* e) {
    if (e->ContextRecord->Eip != DWORD(uintptr_t(uagfx_safe_copy_at))) return EXCEPTION_CONTINUE_SEARCH;
    const DWORD code = e->ExceptionRecord->ExceptionCode;
    if (code != EXCEPTION_ACCESS_VIOLATION && code != EXCEPTION_IN_PAGE_ERROR && code != STATUS_GUARD_PAGE_VIOLATION)
        return EXCEPTION_CONTINUE_SEARCH;
    e->ContextRecord->Eip = DWORD(uintptr_t(uagfx_safe_copy_fault));
    InterlockedIncrement(&g_faults);
    return EXCEPTION_CONTINUE_EXECUTION;
}

bool start() {
    if (!AddVectoredExceptionHandler(1, onCopyFault)) {
        LOG_INFO("game memory: no fault handler - read by ReadProcessMemory as before");
        return false;
    }
    // The check: a known value read, then a reserved page (no access) answering false
    const uint32_t known = 0x5A17C0DE;
    uint32_t got = 0, none = 0;
    const bool read = uagfx_safe_copy(&got, &known, sizeof got) == 1 && got == known;
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE, PAGE_NOACCESS);
    const bool refused = page && uagfx_safe_copy(&none, page, sizeof none) == 0;
    if (page) VirtualFree(page, 0, MEM_RELEASE);
    if (!refused || !read) {
        LOG_INFO("game memory: the processor's own copy failed its check (%s) - read by ReadProcessMemory as before",
                 !read ? "a known value" : "a page with no access");
        return false;
    }
    LOG_INFO("game memory: read by the processor's own copy (an address gone answers false through a fault handler)");
    return true;
}

}  // namespace

bool ready() {
    static const bool on = start();
    return on;
}

bool copy(void* out, uint32_t from, size_t size) {
    return uagfx_safe_copy(out, reinterpret_cast<const void*>(uintptr_t(from)), size) != 0;
}

unsigned takeFaults() { return unsigned(InterlockedExchange(&g_faults, 0)); }

bool readable(uint32_t at, size_t size) {
    if (!at || !size || uint64_t(at) + size > 0x100000000ull) return false;
    const uint32_t last = at + uint32_t(size - 1);
    if (ready()) {
        uint8_t byte = 0;
        for (uint32_t page = at & ~0xFFFu;; page += 0x1000) {  // a byte of each page: the first's at `at`
            if (!copy(&byte, page < at ? at : page, 1)) return false;
            if (page >= (last & ~0xFFFu)) break;
        }
        return copy(&byte, last, 1);
    }
    const DWORD kRead = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE |
                        PAGE_EXECUTE_WRITECOPY;
    for (uintptr_t p = at; p <= last;) {
        MEMORY_BASIC_INFORMATION m{};
        if (!VirtualQuery(reinterpret_cast<LPCVOID>(p), &m, sizeof m) || m.State != MEM_COMMIT ||
            (m.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || !(m.Protect & kRead))
            return false;
        p = reinterpret_cast<uintptr_t>(m.BaseAddress) + m.RegionSize;
        if (p == 0) break;  // the address space's end
    }
    return true;
}

}  // namespace uagfx::win::saferead
