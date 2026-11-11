#include "win/hero_picture.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <vector>

#include "core/sprite_file.hpp"
#include "win/game_camera.hpp"
#include "win/head_picture.hpp"
#include "win/pack_frames.hpp"

namespace uagfx::win::heropicture {
namespace {

constexpr uint32_t kSession = 0x771148;
constexpr uint32_t kBodies = kSession + 0x830, kFolders = kSession + 0x840, kAccessories = kSession + 0x880;
constexpr uint32_t kEndings = kSession + 0x8A0;
constexpr uint32_t kJob = 0x771F8C, kSex = 0x772DEC, kHead = 0x7718F4, kLow = 0x771900, kTop = 0x771904;
constexpr uint32_t kMiddle = 0x771908, kColour = 0x771910, kOptions = 0x77191C;
constexpr int kMostView = 4096;
const std::string kBody = "\xC0\xCE\xB0\xA3\xC1\xB7\\\xB8\xF6\xC5\xEB\\";  // 인간족\몸통\ (CP949)
const std::string kAccessory = "\xBE\xC7\xBC\xBC\xBB\xE7\xB8\xAE\\";        // 악세사리\ (CP949)
const std::string kEffect = "\xC0\xCC\xC6\xD1\xC6\xAE\\";                   // 이팩트\ (CP949)
const std::string kFalcon = "\xB8\xC5";                                      // 매
const std::string kCart = "\xBC\xD5\xBC\xF6\xB7\xB9";                        // 손수레

uint32_t read32(uint32_t at) {
    uint32_t v = 0;
    return readGame(at, &v, 4) ? v : 0;
}

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
           middle == o.middle && options == o.options;
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
    Loaded body = load(bodyBase);
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
    std::vector<Loaded> gear;  // the low, the middle, the top headgear (the game's order over the head)
    for (const int view : {l.low, l.middle, l.top}) {
        if (view <= 0 || view > kMostView) continue;
        const std::string acc = nameAt(kAccessories, view);
        if (acc.empty()) continue;
        Loaded g = load("data\\sprite\\" + kAccessory + folder + "\\" + folder + acc);
        if (g.ok) gear.push_back(std::move(g));
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
        g.big = packframes::of(g.spr, g.sprite, packframes::usedBy(g.sprite, g.act, size_t(dir), 0), factor);
        if (g.big.factor) factor = g.big.factor, ++bigParts;
    }
    const auto from4 = [](const Loaded& p) -> std::function<const spritefile::Image*(int)> {
        if (!p.big.factor) return nullptr;
        return [&p](int index) { return p.big.at(index); };
    };
    std::vector<spritefile::Part> parts;  // under to over: the cart, the body, the head, its headgear, the falcon
    if (cart.ok) parts.push_back({&cart.sprite, &cart.act, size_t(dir), 0, false, 0, 0, false, from4(cart)});
    parts.push_back({&body.sprite, &body.act, size_t(dir), 0, false, 0, 0, true, from4(body)});
    if (head.ok) parts.push_back({&head.sprite, &head.act, size_t(dir), 0, true, 0, 0, false, from4(head)});
    for (const Loaded& g : gear) parts.push_back({&g.sprite, &g.act, size_t(dir), 0, true, 0, 0, false, from4(g)});
    if (falcon.ok) {  // perched: its feet (its frame's foot, 22 under its origin) on the shoulder beside the neck
        int nx = 0, ny = -70;
        const auto& frames = body.act.actions;
        if (size_t(dir) < frames.size() && !frames[size_t(dir)].empty() && !frames[size_t(dir)][0].anchors.empty())
            nx = frames[size_t(dir)][0].anchors[0].first, ny = frames[size_t(dir)][0].anchors[0].second;
        const int side = dir == 2 || dir == 6 ? 0 : dir >= 3 && dir <= 5 ? -12 : 12;  // the right shoulder
        parts.push_back(
            {&falcon.sprite, &falcon.act, size_t(dir), 0, false, nx + side, ny + 6 - 22, false, from4(falcon)});
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
        *from = bodyBase + (head.ok ? " + head" : "") + " + " + std::to_string(gear.size()) + " headgear(s)" +
                (cart.ok ? " + cart" : "") + (falcon.ok ? " + falcon" : "") +
                (factor > 1 ? ", the AI pack's frames " + std::to_string(factor) + "x for " + std::to_string(bigParts) +
                                  " part(s)"
                            : ", the game's own pixels") +
                ", made " + std::to_string(madeW) + "x" + std::to_string(madeH) + ", shows " + std::to_string(im.w) +
                "x" + std::to_string(im.h) + ", drawn " + std::to_string(out.width) + "x" + std::to_string(out.height);
    return out.width > 0;
}

}  // namespace uagfx::win::heropicture
