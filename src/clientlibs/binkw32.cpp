// 3.0.1-beta.5 (the user: our own libraries in place of the client's old ones): binkw32.dll - RAD Game Tools' Bink
// video stands in for nothing in uaRO: the game only ever opens openning.bik (not in the client) and goes on when
// BinkOpen answers NULL (uaRO.exe 0x50B5A8, 0x50B7C7). The 11 functions it takes, by their decorated names (stdcall,
// binkw32.def); none of the others is reached without a video.
#include <windows.h>

extern "C" {

// Told from RAD's binkw32.dll by these bytes (core/client_libs.hpp)
__attribute__((used)) const char kUaroMark[] = "uaRO Graphics client library: binkw32.dll (no video)";

void* __stdcall BinkOpen(const char*, unsigned) { return nullptr; }  // no video: the game goes on
void __stdcall BinkClose(void*) {}
int __stdcall BinkDoFrame(void*) { return 0; }
void __stdcall BinkNextFrame(void*) {}
int __stdcall BinkCopyToBuffer(void*, void*, int, unsigned, unsigned, unsigned, unsigned) { return 0; }
int __stdcall BinkDDSurfaceType(void*) { return -1; }  // a surface it cannot draw on
int __stdcall BinkWait(void*) { return 0; }
int __stdcall BinkPause(void*, int) { return 0; }
void __stdcall BinkGoto(void*, unsigned, int) {}
void* __stdcall BinkOpenDirectSound(unsigned long) { return nullptr; }
int __stdcall BinkSetSoundSystem(void*, unsigned long) { return 1; }

BOOL WINAPI DllMain(HINSTANCE, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        volatile const char* mark = kUaroMark;  // kept in the file
        (void)mark;
    }
    return TRUE;
}

}  // extern "C"
