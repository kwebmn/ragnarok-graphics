#include "pad/win/client_bar.hpp"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "pad/win/client_memory.hpp"

namespace uapad::clientbar {
namespace {

using clientmem::load;
using clientmem::matches;
using clientmem::readable;

// uaRO.exe / uaRO-1.1.exe: the CSession object and the parts of it we read.
constexpr uintptr_t kSession = 0x771148;
constexpr uintptr_t kSlotNames = kSession + 0x314;   // std::string[27], 16 bytes each
constexpr uintptr_t kSlotLevels = kSession + 0x4c4;  // int[27]
constexpr uintptr_t kInventory = kSession + 0xf1c;   // std::list<ITEM_INFO>: its sentinel node
// A list node: next at +0, ITEM_INFO at +8 (m_itemIndex +0x08, m_itemName +0x2c, m_isIdentified +0x3c).
constexpr uintptr_t kNodeIndex = 8 + 0x08;
constexpr uintptr_t kNodeName = 8 + 0x2c;
constexpr uintptr_t kNodeIdentified = 8 + 0x3c;
constexpr size_t kMaxName = 64;
constexpr int kMaxItems = 512;

// The functions that use those offsets, byte for byte (CSession::GetShortcutName, GetItemInfo,
// SetShortcutLevel): another build fails this check instead of being misread.
const uint8_t kSigName[] = {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x08, 0xc1, 0xe0,
                            0x04, 0x8d, 0x84, 0x08, 0x14, 0x03, 0x00, 0x00};
const uint8_t kSigItems[] = {0x55, 0x8b, 0xec, 0x51, 0x8b, 0x81, 0x20, 0x0f, 0x00, 0x00};
const uint8_t kSigLevel[] = {0x55, 0x8b, 0xec, 0x8b, 0x45, 0x0c, 0x8b, 0x55,
                             0x08, 0x89, 0x94, 0x81, 0xc4, 0x04, 0x00, 0x00};
// The shortcut window asking for a slot name: mov ecx, 0x771148; call GetShortcutName — the session's address.
const uint8_t kSigSession[] = {0xb9, 0x48, 0x11, 0x77, 0x00, 0xe8, 0x1e, 0x94, 0x1b, 0x00};

// An MSVC 6 std::string: its c_str pointer at +4, its length at +8.
bool loadString(uintptr_t obj, std::string& out) {
    uint32_t ptr = 0;
    uint32_t len = 0;
    if (!load(obj + 4, ptr) || !load(obj + 8, len)) return false;
    out.clear();
    if (ptr == 0 || len == 0) return true;
    if (len > kMaxName || !readable(ptr, len)) return false;
    out.assign(reinterpret_cast<const char*>(uintptr_t(ptr)), len);
    return true;
}

// Item names are in the system code page; the pad's log and menu speak UTF-8.
std::string toUtf8(const std::string& acp) {
    if (acp.empty()) return acp;
    const int wn = MultiByteToWideChar(CP_ACP, 0, acp.data(), int(acp.size()), nullptr, 0);
    if (wn <= 0) return acp;
    std::wstring wide(size_t(wn), L'\0');
    MultiByteToWideChar(CP_ACP, 0, acp.data(), int(acp.size()), &wide[0], wn);
    const int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), wn, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return acp;
    std::string utf8(size_t(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), wn, &utf8[0], n, nullptr, nullptr);
    return utf8;
}

// One pass over the client's inventory list: each identified item name with the inventory index of its
// first entry, as the client resolves a bar item. One readability check per node keeps this cheap.
std::vector<std::pair<std::string, uint16_t>> inventoryNames() {
    std::vector<std::pair<std::string, uint16_t>> names;
    uint32_t head = 0;
    uint32_t node = 0;
    if (!load(kInventory, head) || head == 0 || !load(head, node)) return names;
    for (int i = 0; i < kMaxItems && node != 0 && node != head; ++i) {
        uint8_t raw[kNodeIdentified + 4];
        if (!readable(node, sizeof raw)) break;
        std::memcpy(raw, reinterpret_cast<const void*>(uintptr_t(node)), sizeof raw);
        uint32_t next = 0;
        int32_t index = 0;
        uint32_t namePtr = 0;
        uint32_t nameLen = 0;
        std::memcpy(&next, raw, 4);
        std::memcpy(&index, raw + kNodeIndex, 4);
        std::memcpy(&namePtr, raw + kNodeName + 4, 4);
        std::memcpy(&nameLen, raw + kNodeName + 8, 4);
        if (raw[kNodeIdentified] && index > 0 && index < 0x10000 && namePtr && nameLen && nameLen <= kMaxName &&
            readable(namePtr, nameLen)) {
            std::string name(reinterpret_cast<const char*>(uintptr_t(namePtr)), nameLen);
            bool seen = false;
            for (const auto& n : names) seen = seen || n.first == name;
            if (!seen) names.emplace_back(std::move(name), uint16_t(index));
        }
        node = next;
    }
    return names;
}

}  // namespace

bool supported() {
    return matches(kSigName, sizeof kSigName, 0x627bb0) && matches(kSigItems, sizeof kSigItems, 0x629140) &&
           matches(kSigLevel, sizeof kSigLevel, 0x6425f0) && matches(kSigSession, sizeof kSigSession, 0x46e788);
}

bool read(std::array<ClientBarSlot, kClientBarSlots>& out) {
    std::vector<std::pair<std::string, uint16_t>> items;
    bool itemsRead = false;
    for (size_t j = 0; j < out.size(); ++j) {
        std::string name;
        int32_t level = 0;
        if (!loadString(kSlotNames + 16 * j, name) || !load(kSlotLevels + 4 * j, level)) return false;
        ClientBarSlot slot;
        if (name.size() > 2 && name[0] == '#' && name[1] == '#') {  // "##AC_DOUBLE"
            slot.kind = ClientBarSlot::Kind::Skill;
            slot.name = name.substr(2);
            slot.level = level > 0 && level < 100 ? uint16_t(level) : 0;
        } else if (!name.empty() && name != "nothing") {
            slot.kind = ClientBarSlot::Kind::Item;
            if (!itemsRead) {
                items = inventoryNames();
                itemsRead = true;
            }
            for (const auto& item : items)  // matched on the client's own spelling
                if (item.first == name) slot.itemIndex = item.second;
            slot.name = toUtf8(name);
        }
        out[j] = slot;
    }
    return true;
}

}  // namespace uapad::clientbar
