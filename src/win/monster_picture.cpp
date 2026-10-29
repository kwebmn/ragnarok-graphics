#include "win/monster_picture.hpp"

#include <memory>
#include <vector>

#include "core/sprite_file.hpp"
#include "win/game_files.hpp"
#include "win/paths.hpp"
#include "win/updates_hook.hpp"

namespace uagfx::win {
namespace {

const std::string kMonsters = "\xB8\xF3\xBD\xBA\xC5\xCD";  // 몬스터

bool readFile(const std::string& entry, std::string& out) {
    if (readGameFile(entry, out)) return true;  // 3.0.11-beta.15: as the game finds it (its zips too)
    static std::unique_ptr<gamefiles::Reader> files;
    if (!files) files = std::make_unique<gamefiles::Reader>(gameDir());
    std::vector<uint8_t> bytes;
    if (!files->read(entry, bytes, nullptr)) return false;
    out.assign(bytes.begin(), bytes.end());
    return true;
}

}  // namespace

bool monsterPicture(const refindex::Mob& mob, int box, panel::Picture& out, std::string* from) {
    for (const std::string& name : refindex::spriteNames(mob))
        for (const std::string& folder : {kMonsters, std::string("npc")}) {
            const std::string base = "data\\sprite\\" + folder + "\\" + name;
            std::string spr, act;
            spritefile::Sprite sp;
            spritefile::Act ac;
            spritefile::Image im;
            if (!readFile(base + ".spr", spr) || !readFile(base + ".act", act) || !spritefile::readSpr(spr, sp) ||
                !spritefile::readAct(act, ac) || !spritefile::frameImage(sp, ac, 0, 0, im))
                continue;
            const spritefile::Image fit = spritefile::fitted(im, box, box);
            out.width = fit.w;
            out.height = fit.h;
            out.px = fit.argb;
            if (from) *from = base + ".spr (" + std::to_string(im.w) + "x" + std::to_string(im.h) + ")";
            return true;
        }
    return false;
}

}  // namespace uagfx::win
