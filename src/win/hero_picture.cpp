#include "win/hero_picture.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include "core/sprite_file.hpp"
#include "win/game_camera.hpp"
#include "win/game_memory.hpp"
#include "win/head_picture.hpp"
#include "win/pack_frames.hpp"
#include "core/weapon_look.hpp"
#include "core/wings.hpp"

namespace uagfx::win::heropicture {
namespace {

constexpr uint32_t kSession = 0x771148;
constexpr uint32_t kBodies = kSession + 0x830, kFolders = kSession + 0x840, kAccessories = kSession + 0x880;
constexpr uint32_t kEndings = kSession + 0x8A0;
constexpr uint32_t kJob = 0x771F8C, kSex = 0x772DEC, kHead = 0x7718F4, kLow = 0x771900, kTop = 0x771904;
constexpr uint32_t kMiddle = 0x771908, kColour = 0x771910, kOptions = 0x77191C;
// 3.0.18-beta.126 (item 3: as uaRO.exe FUN_0046fd40 draws it): the clothes' dye (0x77190C) by the session's +0x90c
// names - "몸\%s%s_%d.pal" (FUN_006399e0) under data\palette\; the job's .imf by its +0x870 names - "%s%s.imf"
// (FUN_0062e920) under data\imf\: the body over the head where its priority is the higher
constexpr uint32_t kCloth = 0x77190C, kBodyPalettes = kSession + 0x90C, kImfs = kSession + 0x870;
const std::string kBodyPal = "\xB8\xF6\\";  // 몸\ (CP949)
// 3.0.18-beta.129 (item 6, the owner's own - the game's equipment window draws none): the weapon and the shield as the
// world draws them - the player's actor (the game mode 0x74B57C, its world +0xC4, the player +0x3C): its weapon
// (+0x368, the item's number in the low half) and shield (+0x378); the weapon's sprite «인간족\<job
// folder><sex>_<n>», else by its kind (FUN_005b1ee0: the globals 0x7719D8 folders, 0x7719E8 sexes, 0x7719F8 kinds'
// names); the shield's «방패\<folder><sex><name>» (FUN_00639c70: the session's +0x890 folders, +0x8F0 names)
constexpr uint32_t kModeNow = 0x74B57C, kModeWorld = 0xC4, kWorldPlayer = 0x3C, kActorWeapon = 0x368;
constexpr uint32_t kActorShield = 0x378, kWeaponFolders = 0x7719D8, kWeaponSexes = 0x7719E8, kWeaponKinds = 0x7719F8;
constexpr uint32_t kShieldFolders = kSession + 0x890, kShieldNames = kSession + 0x8F0;
const std::string kHumanRace = "\xC0\xCE\xB0\xA3\xC1\xB7\\";  // 인간족\ (CP949)
const std::string kShieldDir = "\xB9\xE6\xC6\xD0\\";           // 방패\ (CP949)
constexpr int kMostView = 4096;
const std::string kBody = "\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\";  // 인간족\몸통\ (CP949)
const std::string kAccessory = "\xBE\xC7\xBC\xBC\xBB\xE7\xB8\xAE\\";        // 악세사리\ (CP949)
const std::string kEffect = "\xC0\xCC\xC6\xD1\xC6\xAE\\";                   // 이팩트\ (CP949)
const std::string kFalcon = "\xB8\xC5";                                      // 매
const std::string kCart = "\xBC\xD5\xBC\xF6\xB7\xB9";                        // 손수레

std::string readString(uint32_t at) {
    char buf[96] = {};
    if (!at || !readGame(at, buf, sizeof buf - 1)) return "";
    buf[sizeof buf - 1] = 0;
    return buf;
}

// A table of the session's names (FUN_0062e840's: past 0xF6F its index less 0xF6E)
std::string nameAt(uint32_t table, int index) {
    if (index < 0 || index > 0x2000) return "";
    const int at = index < 0xF6F ? index : index - 0xF6E;
    const uint32_t base = read32(table);
    return base ? readString(read32(base + 4 * uint32_t(at))) : "";
}

// A global table of names (a pointer to its array; past 0xF6F its index less 0xF6E)
std::string globalAt(uint32_t table, int index) {
    if (index < 0 || index > 0x2000) return "";
    const int at = index < 0xF6F ? index : index - 0xF6E;
    const uint32_t base = read32(table);
    return base ? readString(read32(base + 4 * uint32_t(at))) : "";
}

struct Loaded {
    spritefile::Sprite sprite;
    spritefile::Act act;
    std::string spr;  // its file's bytes: the AI pack's frames are found by them
    packframes::Frames big;
    bool ok = false;
};

Loaded load(const std::string& base) {
    Loaded l;
    std::string act;
    l.ok = headpicture::readFile(base + ".spr", l.spr) && headpicture::readFile(base + ".act", act) &&
           spritefile::readSpr(l.spr, l.sprite) && spritefile::readAct(act, l.act);
    return l;
}

}  // namespace

bool Looks::operator==(const Looks& o) const {
    return job == o.job && sex == o.sex && head == o.head && colour == o.colour && low == o.low && top == o.top &&
           middle == o.middle && options == o.options && cloth == o.cloth && weapon == o.weapon &&
           shield == o.shield;
}

int shownJob(int job, uint32_t options) {
    const bool riding = (options >> 5 & 1) != 0;
    if (riding) {
        if (job == 7) return 0xD;
        if (job == 0xE) return 0x15;
        if (job == 0xFA8) return 0xFAE;
        if (job == 0xFBE) return 0xFC4;
        if (job == 0xFAF) return 0xFB6;
        if (job == 0xFC5) return 0xFCC;
    }
    if (job == 0xFCF && (options >> 15 & 1) != 0) return 0xFD0;
    return job;
}

Looks own() {
    Looks l;
    l.job = int(read32(kJob));
    l.sex = read32(kSex) != 0 ? 1 : 0;
    l.head = int(read32(kHead));
    l.colour = int(read32(kColour));
    l.low = int(read32(kLow));
    l.top = int(read32(kTop));
    l.middle = int(read32(kMiddle));
    l.options = read32(kOptions);
    l.cloth = int(read32(kCloth));
    const uint32_t mode = read32(kModeNow), world = mode ? read32(mode + kModeWorld) : 0;
    const uint32_t player = world ? read32(world + kWorldPlayer) : 0;
    if (player) {  // 3.0.18-beta.129
        l.weapon = int(read32(player + kActorWeapon) & 0xFFFF);
        l.shield = int(read32(player + kActorShield));
    }
    return l;
}

bool make(const Looks& l, int direction, int w, int h, panel::Picture& out, std::string* from) {
    out = panel::Picture{};
    const int dir = ((direction % 8) + 8) % 8;
    const int sex = l.sex != 0 ? 1 : 0;
    const std::string folder = nameAt(kFolders, sex), ending = nameAt(kEndings, sex);
    const std::string bodyName = nameAt(kBodies, shownJob(l.job, l.options));
    if (folder.empty() || bodyName.empty()) {
        if (from) *from = "no body name for job " + std::to_string(l.job);
        return false;
    }
    const std::string bodyBase = "data\\sprite\\" + kBody + folder + "\\" + bodyName + ending;
    Loaded body;
    {  // 3.0.18-beta.126: the clothes' dye in the body's palette, as the game
        std::string act;
        body.ok = headpicture::readFile(bodyBase + ".spr", body.spr) && headpicture::readFile(bodyBase + ".act", act);
        const std::string palName = nameAt(kBodyPalettes, shownJob(l.job, l.options));
        std::string pal;
        if (body.ok && l.cloth > 0 && l.cloth < 1000 && !palName.empty() &&
            headpicture::readFile("data\\palette\\" + kBodyPal + palName + ending + "_" + std::to_string(l.cloth) +
                                      ".pal",
                                  pal) &&
            pal.size() >= 1024 && body.spr.size() > 1024)
            body.spr.replace(body.spr.size() - 1024, 1024, pal, 0, 1024);
        body.ok = body.ok && spritefile::readSpr(body.spr, body.sprite) && spritefile::readAct(act, body.act);
    }
    if (!body.ok) {
        if (from) *from = "the body " + bodyBase + " not read";
        return false;
    }
    Loaded head;
    {
        std::string act;
        head.ok = headpicture::headFiles(sex, l.head, l.colour, head.spr, act) &&
                  spritefile::readSpr(head.spr, head.sprite) && spritefile::readAct(act, head.act);
    }
    Loaded gear[3];  // the low, the middle, the top headgear (the game's order over the head)
    int gears = 0;
    for (int i = 0; i < 3; ++i) {
        const int view = i == 0 ? l.low : i == 1 ? l.middle : l.top;
        // 3.0.18-beta.233: the wings' items show no headgear of their own (core/wings)
        if (view <= 0 || view > kMostView || uagfx::wings::lookOfView(view) != uagfx::wings::Look::None) continue;
        const std::string acc = nameAt(kAccessories, view);
        if (acc.empty()) continue;
        gear[i] = load("data\\sprite\\" + kAccessory + folder + "\\" + folder + acc);
        if (gear[i].ok) ++gears;
    }
    // the cart behind (0x08 the first, 0x80 0x100 0x200 0x400 the others), the falcon over (0x10)
    Loaded cart, falcon;
    const uint32_t carts[5] = {0x08, 0x80, 0x100, 0x200, 0x400};
    for (int i = 0; i < 5; ++i)
        if (l.options & carts[i]) {
            cart = load("data\\sprite\\" + kEffect + kCart + (i == 0 ? "" : std::to_string(i)));
            break;
        }
    if (l.options & 0x10) falcon = load("data\\sprite\\" + kEffect + kFalcon);
    // 3.0.18-beta.114 (the owner: «картинка взята не с апскейленой версии. тоисть квадратная»): each part's frames from
    // the AI texture pack (4x, as the game shows them with it), one size for them all; a part without - stretched
    int factor = 0, bigParts = 0;
    for (Loaded* p : {&body, &head, &cart, &falcon}) {
        if (!p->ok) continue;
        p->big = packframes::of(p->spr, p->sprite, packframes::usedBy(p->sprite, p->act, size_t(dir), 0), factor);
        if (p->big.factor) factor = p->big.factor, ++bigParts;
    }
    for (Loaded& g : gear) {
        if (!g.ok) continue;
        g.big = packframes::of(g.spr, g.sprite, packframes::usedBy(g.sprite, g.act, size_t(dir), 0), factor);
        if (g.big.factor) factor = g.big.factor, ++bigParts;
    }
    const auto from4 = [](const Loaded& p) -> std::function<const spritefile::Image*(int)> {
        if (!p.big.factor) return nullptr;
        return [&p](int index) { return p.big.at(index); };
    };
    // 3.0.18-beta.129: the weapon (its own sprite, else its kind's) and the shield
    Loaded weapon, shield;
    if (l.weapon > 0) {
        const int job = shownJob(l.job, l.options);
        const std::string folder = globalAt(kWeaponFolders, job), sx = globalAt(kWeaponSexes, sex);
        if (!folder.empty()) {
            weapon = load("data\\sprite\\" + kHumanRace + folder + sx + "_" + std::to_string(l.weapon));
            const int kind = weaponlook::forJob(job, weaponlook::kindOf(l.weapon));
            if (!weapon.ok && kind > 0) {
                const std::string kindName = globalAt(kWeaponKinds, kind);
                if (!kindName.empty()) weapon = load("data\\sprite\\" + kHumanRace + folder + sx + kindName);
            }
        }
    }
    if (l.shield > 0) {
        const int job = shownJob(l.job, l.options);
        const std::string folder = nameAt(kShieldFolders, job), name = nameAt(kShieldNames, l.shield);
        if (!folder.empty() && !name.empty())
            shield = load("data\\sprite\\" + kShieldDir + folder + ending + name);
    }
    for (Loaded* p : {&weapon, &shield}) {
        if (!p->ok) continue;
        p->big = packframes::of(p->spr, p->sprite, packframes::usedBy(p->sprite, p->act, size_t(dir), 0), factor);
        if (p->big.factor) factor = p->big.factor, ++bigParts;
    }
    // 3.0.18-beta.126: the job's .imf - the body over the head in this frame; .227 (R-UC9NT7): as the world reads it
    // (the body's priority 0, the head's 1 - .126 had it the other way: the body's neck over the mouth)
    bool bodyOver = false;
    {
        const std::string imfName = nameAt(kImfs, shownJob(l.job, l.options));
        std::string bytes;
        spritefile::Imf imf;
        if (!imfName.empty() && headpicture::readFile("data\\imf\\" + imfName + ending + ".imf", bytes) &&
            spritefile::readImf(bytes, imf))
            bodyOver = spritefile::bodyOverHead(imf, size_t(dir), 0);
    }
    std::vector<spritefile::Part> parts;  // under to over: spritefile::heroLayers
    const auto put = [&](const Loaded& p, bool head) {
        if (p.ok) parts.push_back({&p.sprite, &p.act, size_t(dir), 0, head, 0, 0, false, from4(p)});
    };
    for (const spritefile::HeroLayer layer : spritefile::heroLayers(bodyOver)) {
        using L = spritefile::HeroLayer;
        switch (layer) {
            case L::Cart: put(cart, false); break;
            case L::Body:
                parts.push_back({&body.sprite, &body.act, size_t(dir), 0, false, 0, 0, true, from4(body)});
                break;
            case L::Head: put(head, true); break;
            case L::Low: put(gear[0], true); break;
            case L::Middle: put(gear[1], true); break;
            case L::Top: put(gear[2], true); break;
            // 3.0.18-beta.129: the weapon and the shield where the body stands (their frames share its origin)
            case L::Weapon: put(weapon, false); break;
            case L::Shield: put(shield, false); break;
            case L::Falcon:
                if (falcon.ok) {  // perched: its feet (its frame's foot, 22 under its origin) on the shoulder
                    int nx = 0, ny = -70;
                    const auto& frames = body.act.actions;
                    if (size_t(dir) < frames.size() && !frames[size_t(dir)].empty() &&
                        !frames[size_t(dir)][0].anchors.empty())
                        nx = frames[size_t(dir)][0].anchors[0].first, ny = frames[size_t(dir)][0].anchors[0].second;
                    const int side = dir == 2 || dir == 6 ? 0 : dir >= 3 && dir <= 5 ? -12 : 12;  // the right one
                    parts.push_back({&falcon.sprite, &falcon.act, size_t(dir), 0, false, nx + side, ny + 6 - 22,
                                     false, from4(falcon)});
                }
                break;
        }
    }
    spritefile::Image im;
    int footX = 0, footY = 0;
    if (!spritefile::compose(parts, im, footX, footY, std::max(1, factor))) {
        if (from) *from = "nothing drawn of " + bodyBase;
        return false;
    }
    // 3.0.18-beta.115 (R-RJBU55 «спрайт маленький», small at the top of its box in .113 too): cut to what shows - the
    // parts' frames' empty room round it was fitted into the box with it
    const int madeW = im.w, madeH = im.h;
    im = spritefile::cutToShown(im, 8);
    if (im.w <= 0) {
        if (from) *from = "nothing shows of " + bodyBase;
        return false;
    }
    spritefile::Image fit;
    if (factor > 1) {  // the pack's: shrunk smoothly to the box (never grown past its 4x)
        fit = spritefile::fitted(im, std::min(w, im.w), std::min(h, im.h));
    } else {
        // 3.0.18-beta.113 (R-55WR4R: a small box left it at its own size): a whole step up to cover the box (sharp
        // pixels; shown fitted into the box), its own size alone - never padded round
        const float cover = std::min(float(w) / float(std::max(1, im.w)), float(h) / float(std::max(1, im.h)));
        const int f = std::clamp(int(std::ceil(cover)), 1, 3);
        fit = spritefile::fitted(im, im.w * f, im.h * f);
    }
    out.width = fit.w;
    out.height = fit.h;
    out.px = fit.argb;
    if (from)
        *from = bodyBase + (l.cloth > 0 ? " dyed " + std::to_string(l.cloth) : "") +
                (bodyOver ? " (over the head)" : "") +
                (head.ok ? " + head" : "") + " + " + std::to_string(gears) + " headgear(s)" +
                (cart.ok ? " + cart" : "") + (falcon.ok ? " + falcon" : "") +
                (weapon.ok  ? " + weapon " + std::to_string(l.weapon)
                 : l.weapon ? " (weapon " + std::to_string(l.weapon) + " not found)"
                            : "") +
                (shield.ok ? " + shield" : l.shield ? " (shield " + std::to_string(l.shield) + " not found)" : "") +
                (factor > 1 ? ", the AI pack's frames " + std::to_string(factor) + "x for " + std::to_string(bigParts) +
                                  " part(s)"
                            : ", the game's own pixels") +
                ", made " + std::to_string(madeW) + "x" + std::to_string(madeH) + ", shows " + std::to_string(im.w) +
                "x" + std::to_string(im.h) + ", drawn " + std::to_string(out.width) + "x" + std::to_string(out.height);
    return out.width > 0;
}

}  // namespace uagfx::win::heropicture
