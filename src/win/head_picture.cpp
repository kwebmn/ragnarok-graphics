#include "win/head_picture.hpp"

#include <memory>
#include <vector>

#include "core/sprite_file.hpp"
#include "win/game_camera.hpp"
#include "win/game_files.hpp"
#include "win/paths.hpp"
#include "win/updates_hook.hpp"

namespace uagfx::win::headpicture {
namespace {

// The session's tables (FUN_0062e7b0's): the heads' folders by sex, the men's and the women's heads' names, endings
constexpr uint32_t kSession = 0x771148;
constexpr uint32_t kFolders = kSession + 0x840, kManHeads = kSession + 0x850, kWomanHeads = kSession + 0x860;
constexpr uint32_t kEndings = kSession + 0x8A0;
const std::string kHuman = "\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xD3\xB8\xAE\xC5\xEB\\";  // 인간족\머리통\ (CP949)
const std::string kHairPalettes = "\xB8\xD3\xB8\xAE\\\xB8\xD3\xB8\xAE";            // 머리\머리
const std::string kMan = "\xB3\xB2", kWoman = "\xBF\xA9";                            // 남, 여

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

std::string readString(uint32_t at) {
    char buf[64] = {};
    if (!at || !readGame(at, buf, sizeof buf - 1)) return "";
    buf[sizeof buf - 1] = 0;
    return buf;
}

bool readFile(const std::string& entry, std::string& out) {
    if (readGameFile(entry, out)) return true;  // as the game finds it (its zips too)
    static std::unique_ptr<gamefiles::Reader> files;
    if (!files) files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    if (!files->read(entry, bytes, nullptr)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

}  // namespace

bool make(int sex, int head, int colour, int side, panel::Picture& out, std::string* from) {
    out = panel::Picture{};
    sex = sex != 0 ? 1 : 0;  // as FUN_0062e7b0: not 0 - a man
    if (head <= 0) head = 1;  // FUN_00639900's for none
    std::string folder = readString(read32(read32(kFolders) + 4 * uint32_t(sex)));
    std::string name = readString(read32(read32(sex ? kManHeads : kWomanHeads) + 4 * uint32_t(head)));
    std::string ending = readString(read32(read32(kEndings) + 4 * uint32_t(sex)));
    if (folder.empty() || name.empty()) {  // the plain heads' names, as the game's own "%d_남.spr"
        folder = sex ? kMan : kWoman;
        name = std::to_string(head);
        ending = "_" + folder;
    }
    const std::string base = "data\\sprite\\" + kHuman + folder + "\\" + name + ending;
    std::string spr, act;
    if (!readFile(base + ".spr", spr) || !readFile(base + ".act", act)) {
        if (from) *from = "no head sprite " + base + ".spr";
        return false;
    }
    std::string pal;
    const std::string palName =
        "data\\palette\\" + kHairPalettes + name + ending + "_" + std::to_string(colour) + ".pal";
    if (colour > 0 && readFile(palName, pal) && pal.size() >= 1024 && spr.size() > 1024)
        spr.replace(spr.size() - 1024, 1024, pal, 0, 1024);  // the hair colour in the sprite's palette, as the game
    spritefile::Sprite sp;
    spritefile::Act ac;
    spritefile::Image im;
    if (!spritefile::readSpr(spr, sp) || !spritefile::readAct(act, ac) || !spritefile::frameImage(sp, ac, 0, 0, im)) {
        if (from) *from = "the head sprite " + base + ".spr not read";
        return false;
    }
    const spritefile::Image fit = spritefile::fitted(im, side, side);
    out.width = fit.w;
    out.height = fit.h;
    out.px = fit.argb;
    if (from)
        *from = base + ".spr (" + std::to_string(im.w) + "x" + std::to_string(im.h) + ")" +
                (pal.empty() ? "" : ", the hair colour " + std::to_string(colour));
    return true;
}

}  // namespace uagfx::win::headpicture
