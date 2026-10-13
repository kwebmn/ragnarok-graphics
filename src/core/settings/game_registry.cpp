#include "core/settings/game_registry.hpp"

#include <cctype>

namespace uagfx::settings {

RegValue RegValue::dword(uint32_t v) {
    RegValue r;
    r.type = uint32_t(RegType::Dword);
    r.bytes = {uint8_t(v), uint8_t(v >> 8), uint8_t(v >> 16), uint8_t(v >> 24)};
    return r;
}

RegValue RegValue::sz(const std::string& s) {
    RegValue r;
    r.type = uint32_t(RegType::Sz);
    r.text = s;
    return r;
}

RegValue RegValue::binary(const std::vector<uint8_t>& b) {
    RegValue r;
    r.type = uint32_t(RegType::Binary);
    r.bytes = b;
    return r;
}

std::optional<uint32_t> RegValue::asDword() const {
    if (type != uint32_t(RegType::Dword) || bytes.size() != 4) return std::nullopt;
    return uint32_t(bytes[0]) | uint32_t(bytes[1]) << 8 | uint32_t(bytes[2]) << 16 | uint32_t(bytes[3]) << 24;
}

bool RegValue::operator==(const RegValue& o) const {
    if (type != o.type) return false;
    return isSz() ? text == o.text : bytes == o.bytes;
}

bool NameLess::operator()(const std::string& a, const std::string& b) const {
    const size_t n = a.size() < b.size() ? a.size() : b.size();
    for (size_t i = 0; i < n; ++i) {
        const int x = std::tolower(static_cast<unsigned char>(a[i]));
        const int y = std::tolower(static_cast<unsigned char>(b[i]));
        if (x != y) return x < y;
    }
    return a.size() < b.size();
}

const RegValue* RegCopy::find(const std::string& subkey, const std::string& name) const {
    const auto k = keys.find(subkey);
    if (k == keys.end()) return nullptr;
    const auto v = k->second.find(name);
    return v == k->second.end() ? nullptr : &v->second;
}

std::optional<RegValue> effectiveValue(const GameRegistry& r, const std::string& subkey, const std::string& name) {
    if (const RegValue* v = r.personal.find(subkey, name)) return *v;
    if (const RegValue* v = r.common.find(subkey, name)) return *v;
    return std::nullopt;
}

bool gameKeyExists(const GameRegistry& r) { return r.personal.exists || r.common.exists; }

bool RegWrite::operator==(const RegWrite& o) const {
    return op == o.op && subkey == o.subkey && name == o.name && (op == RegOp::Delete || value == o.value);
}

std::optional<RegWrite> planWrite(const GameRegistry& r, const std::string& subkey, const std::string& name,
                                  const RegValue& v) {
    const RegValue* mine = r.personal.find(subkey, name);
    const RegValue* common = r.common.find(subkey, name);
    if (common && *common == v) {
        if (!mine) return std::nullopt;
        return RegWrite{subkey, name, RegOp::Delete, RegValue{}};
    }
    if (mine && *mine == v) return std::nullopt;
    return RegWrite{subkey, name, RegOp::Set, v};
}

std::optional<RegWrite> planReset(const GameRegistry& r, const std::string& subkey, const std::string& name) {
    if (!r.personal.find(subkey, name) || !r.common.find(subkey, name)) return std::nullopt;
    return RegWrite{subkey, name, RegOp::Delete, RegValue{}};
}

void applyWrites(RegCopy& personal, const std::vector<RegWrite>& writes) {
    for (const RegWrite& w : writes) {
        if (w.op == RegOp::Set) {
            personal.exists = true;
            personal.keys[w.subkey][w.name] = w.value;
        } else {
            const auto k = personal.keys.find(w.subkey);
            if (k != personal.keys.end()) k->second.erase(w.name);
        }
    }
}

RegCopy effectiveCopy(const GameRegistry& r) {
    RegCopy out = r.common;
    out.exists = r.personal.exists || r.common.exists;
    for (const auto& key : r.personal.keys)
        for (const auto& v : key.second) out.keys[key.first][v.first] = v.second;
    return out;
}

std::vector<RegWrite> windowLayoutReset(const GameRegistry& r) {
    std::vector<RegWrite> out;
    const RegCopy all = effectiveCopy(r);
    const auto layout = all.keys.find("UIRectInfo");
    if (layout == all.keys.end()) return out;
    const auto same = [](const std::string& a, const std::string& b) { return !NameLess{}(a, b) && !NameLess{}(b, a); };
    const RegValue away = RegValue::dword(0xFFFFFFFFu);
    std::vector<std::string> names{"M_CHATWNDY"};  // the chat's Y, there or not
    for (const auto& v : layout->second)
        if (v.first.size() > 2 && same(v.first.substr(v.first.size() - 2), ".X")) names.push_back(v.first);
    for (const std::string& name : names)
        if (const auto w = planWrite(r, "UIRectInfo", name, away)) out.push_back(*w);
    return out;
}

std::vector<uint8_t> halDeviceGuid() {
    // {84E63DE0-46AA-11CF-816F-0000C020156E}: IID_IDirect3DHALDevice, the GUID's bytes in memory order.
    return {0xE0, 0x3D, 0xE6, 0x84, 0xAA, 0x46, 0xCF, 0x11, 0x81, 0x6F, 0x00, 0x00, 0xC0, 0x20, 0x15, 0x6E};
}

std::vector<RegWrite> firstRunWrites(int width, int height, bool fullscreen) {
    auto dw = [](const char* name, uint32_t v) { return RegWrite{"", name, RegOp::Set, RegValue::dword(v)}; };
    return {
        dw("ISFULLSCREENMODE", fullscreen ? 1 : 0),
        dw("WIDTH", uint32_t(width)),
        dw("HEIGHT", uint32_t(height)),
        dw("BITPERPIXEL", 32),
        RegWrite{"", "GUIDDEVICE", RegOp::Set, RegValue::binary(halDeviceGuid())},
        RegWrite{"", "GUIDDRIVER", RegOp::Set, RegValue::binary(std::vector<uint8_t>(16, 0))},
        RegWrite{"", "DEVICENAME", RegOp::Set, RegValue::sz("Direct3D HAL")},
        dw("SOUNDMODE", 1),
        dw("DIGITALRATETYPE", 0),
        dw("DIGITALBITSTYPE", 0),
        dw("NUMSAMPLETYPE", 0),
        dw("SPEAKERTYPE", 0),
        RegWrite{"", "PROVIDERNAME", RegOp::Set, RegValue::sz("No Provider")},
        dw("ISLIGHTMAP", 1),
        dw("SPRITEMODE", 2),
        dw("TEXTUREMODE", 2),
        dw("TRILINEARFILTER", 0),
        dw("ISVOODOO", 0),
    };
}

namespace {

std::string valueText(const RegValue& v) {
    if (const auto d = v.asDword()) return std::to_string(*d);
    if (v.isSz()) return v.text;
    static const char kHex[] = "0123456789abcdef";
    std::string out;
    for (const uint8_t b : v.bytes) {
        if (!out.empty()) out += ' ';
        out += kHex[b >> 4];
        out += kHex[b & 15];
    }
    return out;
}

}  // namespace

std::vector<CopyDifference> copyDifferences(const GameRegistry& r, const std::vector<std::string>& names) {
    std::vector<CopyDifference> out;
    for (const std::string& name : names) {
        const RegValue* mine = r.personal.find("", name);
        const RegValue* common = r.common.find("", name);
        if (mine && common && *mine != *common) out.push_back({name, valueText(*mine), valueText(*common)});
    }
    return out;
}

const std::vector<std::string>& setupValues() {
    static const std::vector<std::string> names{"ISFULLSCREENMODE", "WIDTH",           "HEIGHT",
                                                "BITPERPIXEL",      "SOUNDMODE",       "DIGITALRATETYPE",
                                                "DIGITALBITSTYPE",  "NUMSAMPLETYPE",   "SPEAKERTYPE",
                                                "PROVIDERNAME"};
    return names;
}

}  // namespace uagfx::settings
