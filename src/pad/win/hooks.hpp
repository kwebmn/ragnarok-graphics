#pragma once
#include <winsock2.h>
#include <windows.h>

#include <cstdint>

namespace uapad::hooks {

// Patches the client's import table (recv, closesocket, GetCursorPos, PeekMessageA) and finds the
// client's own send/recv function-pointer variables.
void install();
void findClientPointers();
// Points the client's send/recv variables at our hooks; returns how many of them go through us now.
int patchClientPointers();
// Sends our own request through the original send(); finishes a partial send right away.
bool sendAll(SOCKET s, const uint8_t* data, int len);
int lastError();

}  // namespace uapad::hooks
