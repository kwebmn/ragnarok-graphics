#include "core/sprite_exe.hpp"

#include <string>

namespace uagfx::sprite_exe {

bool isWriteCall(const uint8_t bytes[5]) {
    if (bytes[0] != 0xE8) return false;
    const uint32_t rel = uint32_t(bytes[1]) | uint32_t(bytes[2]) << 8 | uint32_t(bytes[3]) << 16 |
                         uint32_t(bytes[4]) << 24;
    return kWriteCall + 5 + rel == kWrite;
}

bool cellTexels(const uint8_t* rle, size_t available, int frameW, int frameH, const uint32_t* palette, int cellW,
                int cellH, std::vector<uint16_t>& out) {
    if (!rle || !palette || frameW < 0 || frameH < 0 || frameW > cellW || frameH > cellH || cellW <= 0 || cellH <= 0)
        return false;
    out.assign(size_t(cellW) * size_t(cellH), 0);
    size_t at = 0;
    int run = 0;  // transparent texels still to come (a run goes on into the next row)
    for (int y = 0; y < frameH; ++y)
        for (int x = 0; x < frameW; ++x) {
            if (run > 0) {
                --run;
                continue;
            }
            if (at >= available) return false;
            const uint8_t b = rle[at];
            if (b == 0) {
                if (at + 1 >= available) return false;
                run = int(rle[at + 1]) - 1;
                at += 2;
            } else {
                out[size_t(y) * size_t(cellW) + size_t(x)] = uint16_t(palette[b] & 0xFFFFu);
                at += 1;
            }
        }
    return true;
}

bool sameColours(const uint32_t* a, const uint32_t* b) {
    for (int i = 0; i < 256; ++i)
        if ((a[i] & 0xFFFFu) != (b[i] & 0xFFFFu)) return false;
    return true;
}

namespace {

// The path in lower case with backslashes, as the folders are compared.
std::string normal(const char* path) {
    std::string p;
    for (const char* c = path; *c && p.size() < 512; ++c)
        p += *c >= 'A' && *c <= 'Z' ? char(*c - 'A' + 'a') : *c == '/' ? '\\' : *c;
    return p;
}

const std::string kEffect = "\xC0\xCC\xC6\xD1\xC6\xAE";  // 이팩트

}  // namespace

objects::File objectFile(const char* path) {
    if (!path) return objects::File::None;
    const std::string p = normal(path);
    const auto is = [&p](const std::string& name) {
        const std::string file = kEffect + "\\" + name;
        return p.size() >= file.size() && p.compare(p.size() - file.size(), file.size(), file) == 0 &&
               (p.size() == file.size() || p[p.size() - file.size() - 1] == '\\');
    };
    if (is("torch_01.spr")) return objects::File::Torch;
    if (is("\xB1\xBC\xB6\xD2\xBF\xAC\xB1\xE2.spr")) return objects::File::Smoke;  // 굴뚝연기
    return objects::File::None;
}

Source sourceOf(const char* path) {
    if (!path) return Source::Unknown;
    const std::string p = normal(path);
    // A folder of the path: at its start or after a backslash, and a backslash after it.
    const auto in = [&p](const std::string& folder) {
        for (size_t at = p.find(folder + "\\"); at != std::string::npos; at = p.find(folder + "\\", at + 1))
            if (at == 0 || p[at - 1] == '\\') return true;
        return false;
    };
    const std::string ui = "\xC0\xAF\xC0\xFA\xC0\xCE\xC5\xCD\xC6\xE4\xC0\xCC\xBD\xBA";  // 유저인터페이스
    const std::string monster = "\xB8\xF3\xBD\xBA\xC5\xCD";                            // 몬스터
    const std::string item = "\xBE\xC6\xC0\xCC\xC5\xDB";                               // 아이템
    const std::string& effect = kEffect;
    const std::string human = "\xC0\xCE\xB0\xA3\xC1\xB7";                              // 인간족: bodies, heads
    const std::string headgear = "\xBE\xC7\xBC\xBC\xBB\xE7\xB8\xAE";                 // 악세사리
    const std::string shield = "\xB9\xE6\xC6\xD0";                                       // 방패
    const std::string garment = "\xB7\xCE\xBA\xEA";                                      // 로브
    // 1.6.7: the emotions (이팩트\emotion.spr), pictures over a head drawn like icons, go with the icons - with the
    // skills' effects (off by default) a pack made with them showed none.
    const size_t name = p.find_last_of('\\') == std::string::npos ? 0 : p.find_last_of('\\') + 1;
    const bool emotion = p.compare(name, 7, "emotion") == 0 && p.size() > 4 && p.compare(p.size() - 4, 4, ".spr") == 0;
    if (p.find("cursors.spr") != std::string::npos || in(ui) || emotion) return Source::Interface;
    if (in(monster) || in("homun") || in("mercenary")) return Source::Monster;
    if (in("npc")) return Source::Npc;
    if (in(item)) return Source::Item;
    if (in(effect)) return Source::Effect;
    if (in(human) || in(headgear) || in(shield) || in(garment)) return Source::Player;
    return Source::Unknown;
}

bool cardOnGround(const char* path) {
    if (!path || sourceOf(path) != Source::Item) return false;
    const std::string p = normal(path), card = "\xC4\xAB\xB5\xE5.spr";  // 카드.spr
    return p.size() >= card.size() && p.compare(p.size() - card.size(), card.size(), card) == 0;
}

const char* sourceName(uint8_t source) {
    switch (Source(source)) {
        case Source::Monster: return "monster";
        case Source::Player: return "player";
        case Source::Npc: return "npc";
        case Source::Item: return "item";
        case Source::Effect: return "effect";
        case Source::Interface: return "interface";
        case Source::Unknown: break;
    }
    return "unknown";
}

}  // namespace uagfx::sprite_exe
