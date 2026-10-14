#pragma once
#include <cstdint>
#include <string>

namespace uagfx::crisp_exe {

// The uaRO.exe stage 6 hooks (docs/research/text.md; crisp text spec §2, §5). Anything else turns crisp text off.
constexpr uint32_t kImageBase = 0x400000;
// CDCBitmap, the game's window picture: its vtable, the ten methods in it, its fields.
constexpr uint32_t kBitmapVtable = 0x6956EC;
constexpr int kBitmapSlots = 10;
constexpr uint32_t kBitmapMethods[kBitmapSlots] = {0x402EB0, 0x403780, 0x402FF0, 0x403010, 0x4037C0,
                                                    0x4030E0, 0x4031B0, 0x403460, 0x403020, 0x403880};
enum Slot { kDestroy = 0, kGetDc = 2, kCopyPixels = 4, kCopyFrom = 5, kSprite = 6, kTextureCopy = 7, kClear = 8,
            kDraw = 9 };
constexpr uint32_t kWidth = 0x04;
constexpr uint32_t kHeight = 0x08;
constexpr uint32_t kDc = 0x0C;
constexpr uint32_t kBits = 0x18;
constexpr uint32_t kDirty = 0x1C;           // a byte: set by every write, cleared by the upload
constexpr uint32_t kTiles = 0x24;           // std::list<CTexture*>: the pointer to its head node
constexpr uint32_t kTileCount = 0x28;
constexpr uint32_t kTextureSurface = 0x0C;  // CTexture: its IDirectDrawSurface7
// The GDI imports of uaRO.exe.
constexpr uint32_t kImportTextOutW = 0x695084;
constexpr uint32_t kImportFillRgn = 0x69507C;
constexpr uint32_t kImportCreateDibSection = 0x6950A0;
constexpr uint32_t kCreateDibReturn = 0x403A16;  // after the only call (FUN_00403960): its ppvBits is &bitmap->bits
// The mode fade's logo (FUN_004ce140): a picture made, copied into, drawn once and deleted on every call - no text.
constexpr uint32_t kLogoCopyReturn = 0x4CE188;  // after its CopyPixels (call at 0x4CE185)
constexpr uint32_t kLogoDrawReturn = 0x4CE1A7;  // after its Draw (call at 0x4CE1A4)
// UIWindowMgr (0x727ED8): the list of top-level windows; UIWindow: the flag Invalidate sets, its children.
constexpr uint32_t kWindowList = 0x727ED8 + 0x38;  // the pointer to the list's head node
constexpr uint32_t kWindowDirty = 0x24;            // a dword
constexpr uint32_t kWindowChildren = 0x0C;         // the pointer to the head node of its children
constexpr uint32_t kWindowDc = 0x28;               // its picture, the CDCBitmap* (0.22.8)
// An MSVC std::list node: next, previous, value.
constexpr uint32_t kNodeNext = 0x00;
constexpr uint32_t kNodeValue = 0x08;

// "" when the ten vtable entries are the known methods, else what differs.
std::string checkBitmapMethods(const uint32_t entries[kBitmapSlots]);

}  // namespace uagfx::crisp_exe
