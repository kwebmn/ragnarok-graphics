#include "win/monster_picture.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "core/mob_pictures.hpp"
#include "core/sprite_file.hpp"
#include "win/game_files.hpp"
#include "win/pack_frames.hpp"
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

// 3.0.11-beta.76 (the owner: «иконки ... и всего остального также по возможности грузить в текстур которые
// обработаны»): the frame's palette pictures from the AI texture pack (win/pack_frames), put together that many times
// bigger; false - the pack has none of them (or the switch is off)
bool biggerFrame(const std::string& spr, const spritefile::Sprite& sp, const spritefile::Act& ac,
                 spritefile::Image& out, int& factor, int& found) {
    const packframes::Frames fr = packframes::of(spr, sp, packframes::usedBy(sp, ac, 0, 0));
    factor = fr.factor;
    found = int(fr.big.size());
    if (!factor) return false;
    return spritefile::frameImageScaled(sp, ac, 0, 0, factor, [&](int index) { return fr.at(index); }, out);
}

}  // namespace

bool monsterPicture(int id, const refindex::Mob& mob, int box, panel::Picture& out, std::string* from) {
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
            int factor = 0, found = 0;
            spritefile::Image big;
            const bool hd = biggerFrame(spr, sp, ac, big, factor, found);
            const spritefile::Image fit = spritefile::fitted(hd ? big : im, box, box);
            out.width = fit.w;
            out.height = fit.h;
            out.px = fit.argb;
            if (from)
                *from = base + ".spr (" + std::to_string(im.w) + "x" + std::to_string(im.h) + ")" +
                        (hd ? ", the AI texture pack's " + std::to_string(found) + " frames " +
                                  std::to_string(factor) + "x"
                            : "");
            return true;
        }
    // 3.0.19-beta.1 (the testers: «чон чон мухи не отображаються так як і 2 МВП»): the client has none of its sprites -
    // the site's picture of it, kept in the driver
    spritefile::Image kept;
    if (!mobpictures::pixels(id, kept.argb, kept.w, kept.h)) return false;
    const spritefile::Image fit = spritefile::fitted(kept, box, box);
    out.width = fit.w;
    out.height = fit.h;
    out.px = fit.argb;
    if (from)
        *from = "no sprite in the client - uaRO Graphics' own picture of it (the site's, " + std::to_string(kept.w) +
                "x" + std::to_string(kept.h) + ")";
    return true;
}

}  // namespace uagfx::win
