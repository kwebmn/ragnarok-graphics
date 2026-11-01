#pragma once
#include <cstddef>
#include <cstdint>

namespace uagfx::login_exe {

// 2.6.17: what uaRO.exe's login and character screens (CLoginMode) hold for the driver's way onto a map
// (core/map_entry.hpp, win/map_entry_hook.hpp) - as this build has it; another build: none of it touched.
constexpr uint32_t kModeNow = 0x74B57C;        // the game's current mode (g_modeMgr + 4)
constexpr uint32_t kLoginVtable = 0x69F4DC;    // CLoginMode's
constexpr uint32_t kVtableStoreAt = 0x59FA3B;  // its constructor: mov dword [esi], kLoginVtable
extern const uint8_t kVtableStoreBytes[6];
constexpr uint32_t kStateAt = 4;               // a CLoginMode's state
constexpr uint32_t kNextStateAt = 0xC;         // the state it goes to next (-1 none): CMode::OnRun calls
                                               // OnChangeState with it at the next turn of its loop, even the same
constexpr int kStateSelecting = 9;             // the character chosen: CH_SELECT_CHAR (0x66) sent
constexpr int kStateEntering = 0xC;            // the map's address had: connected, CZ_ENTER sent, waiting
constexpr uint32_t kConnection = 0x6EE670;     // the game's connection (CConnection), its socket at +4
constexpr uint32_t kConnectionBuilt = 0x6EE4D0;  // bit 1: it was built
constexpr uint32_t kAccount = 0x771F7C;        // the account's id
constexpr uint32_t kChangeState = 0x5A04A0;    // CLoginMode::OnChangeState(state): checked, not called
constexpr uint32_t kClose = 0x418C90;          // CConnection::Disconnect(), thiscall
constexpr uint32_t kQueue = 0x4191B0;          // CConnection::Send(len, data), thiscall
extern const uint8_t kChangeStateBytes[9];     // their first bytes
extern const uint8_t kCloseBytes[9];
extern const uint8_t kQueueBytes[9];
// The state 0xc wait (CLoginMode, 0x5A1BF0): mov edx, [edi+0x1ac]; add edx, 15000; cmp eax, edx - its immediate
constexpr uint32_t kWaitAt = 0x5A1C92;
extern const uint8_t kWaitBytes[14];
constexpr uint32_t kWaitImm = kWaitAt + 8;
extern const uint8_t kPatientImm[4];           // map_entry::kPatientWaitMs
// State 9: get the connection, call kQueue with CH_SELECT_CHAR - that call's place (E8 rel32 at kSelectAt + 7)
constexpr uint32_t kSelectAt = 0x5A17E2;
extern const uint8_t kSelectBytes[14];
constexpr uint32_t kSelectCall = kSelectAt + 7;

}  // namespace uagfx::login_exe
