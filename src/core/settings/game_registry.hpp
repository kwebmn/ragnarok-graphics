#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace uagfx::settings {

// Registry value types (winnt.h numbers) the game and regedit use.
enum class RegType : uint32_t { Sz = 1, ExpandSz = 2, Binary = 3, Dword = 4, MultiSz = 7, Qword = 11 };

// One value. REG_SZ is UTF-8 text up to the first NUL (the Windows layer converts from UTF-16); every other type
// keeps its bytes as Windows gives them.
struct RegValue {
    uint32_t type = uint32_t(RegType::Dword);
    std::string text;
    std::vector<uint8_t> bytes;
    static RegValue dword(uint32_t v);
    static RegValue sz(const std::string& s);
    static RegValue binary(const std::vector<uint8_t>& b);
    bool isSz() const { return type == uint32_t(RegType::Sz); }
    std::optional<uint32_t> asDword() const;  // a 4-byte REG_DWORD only
    bool operator==(const RegValue& o) const;
    bool operator!=(const RegValue& o) const { return !(*this == o); }
};

// Windows compares key and value names without regard to case (ASCII letters; the game's names are ASCII).
struct NameLess {
    bool operator()(const std::string& a, const std::string& b) const;
};
using RegValues = std::map<std::string, RegValue, NameLess>;

// One copy of the game's key: "" is the key itself, "Option", "UIRectInfo", "ShortcutItem\\uaRO" its subkeys.
struct RegCopy {
    bool exists = false;  // the key itself exists in this copy
    std::map<std::string, RegValues, NameLess> keys;
    const RegValue* find(const std::string& subkey, const std::string& name) const;
};

// The game's key as the game sees it (stage 2 spec §2.1, §4.2): the personal copy (the user's VirtualStore) over the
// common one (HKLM, written by setup.exe).
struct GameRegistry {
    RegCopy personal;
    RegCopy common;
};

std::optional<RegValue> effectiveValue(const GameRegistry& r, const std::string& subkey, const std::string& name);
bool gameKeyExists(const GameRegistry& r);  // in either copy

enum class RegOp { Set, Delete };
// One change of the personal copy.
struct RegWrite {
    std::string subkey;
    std::string name;
    RegOp op = RegOp::Set;
    RegValue value;  // for Set
    bool operator==(const RegWrite& o) const;
};

// The change of the personal copy that makes the game see `v` (spec §4.2): none when it already does; deleting the
// personal value when the common copy holds exactly `v` (setup.exe stays in charge of it); else setting it.
std::optional<RegWrite> planWrite(const GameRegistry& r, const std::string& subkey, const std::string& name,
                                  const RegValue& v);
// "As in setup.exe": deleting the personal value, so the game sees the common one; none when there is no personal
// value or no common one to fall back on.
std::optional<RegWrite> planReset(const GameRegistry& r, const std::string& subkey, const std::string& name);
void applyWrites(RegCopy& personal, const std::vector<RegWrite>& writes);

// The IID of the Direct3D HAL device, as setup.exe stores it in GUIDDEVICE.
std::vector<uint8_t> halDeviceGuid();
// What the game needs to start when neither copy holds its key (spec §4.3), with the screen chosen.
std::vector<RegWrite> firstRunWrites(int width, int height, bool fullscreen);

}  // namespace uagfx::settings
