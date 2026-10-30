#include "win/monster_picture.hpp"

#include <algorithm>
#include <map>
#include <memory>
#include <vector>

#include "core/pack_source.hpp"
#include "core/sprite_file.hpp"
#include "win/game_files.hpp"
#include "win/pack_pictures.hpp"
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
// обработаны»): the frame's palette pictures from the AI texture pack (its frames 4x their cache cells), put together
// that many times bigger; false - the pack has none of them (or the switch is off)
bool biggerFrame(const std::string& spr, const spritefile::Sprite& sp, const spritefile::Act& ac,
                 spritefile::Image& out, int& factor, int& found) {
    factor = found = 0;
    if (packpictures::iconsStamp() <= 0 || ac.actions.empty() || ac.actions[0].empty()) return false;
    std::vector<int> used;
    for (const spritefile::Layer& ly : ac.actions[0][0].layers)
        if (ly.type == 0 && ly.index >= 0 && size_t(ly.index) < sp.palette.size()) used.push_back(ly.index);
    if (used.empty()) return false;
    std::vector<pack::Picture> cells;
    if (!pack::spriteFrames(reinterpret_cast<const uint8_t*>(spr.data()), spr.size(), cells, nullptr, &used))
        return false;
    std::map<int, spritefile::Image> big;
    for (const pack::Picture& c : cells) {
        std::vector<uint32_t> argb;
        int w = 0, h = 0;
        if (c.width <= 0 || !packpictures::ofSprite(c.key, argb, w, h) || w % c.width || w / c.width < 2 ||
            h != c.height * (w / c.width))
            continue;
        const int f = w / c.width;
        if (factor && f != factor) continue;  // one size for them all
        factor = f;
        const spritefile::Image& own = sp.palette[size_t(c.frame)];
        spritefile::Image& b = big[c.frame];  // the frame in its cell's top left, 4x
        b.w = own.w * f;
        b.h = own.h * f;
        b.argb.resize(size_t(b.w) * size_t(b.h));
        for (int y = 0; y < b.h; ++y)
            std::copy_n(argb.begin() + std::ptrdiff_t(size_t(y) * size_t(w)), b.w,
                        b.argb.begin() + std::ptrdiff_t(size_t(y) * size_t(b.w)));
    }
    found = int(big.size());
    if (!factor || big.empty()) return false;
    return spritefile::frameImageScaled(sp, ac, 0, 0, factor,
                                        [&](int index) -> const spritefile::Image* {
                                            const auto it = big.find(index);
                                            return it != big.end() ? &it->second : nullptr;
                                        },
                                        out);
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
    return false;
}

}  // namespace uagfx::win
