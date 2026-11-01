// 3.0.11-beta.149 (the owner: «сделай все без остановки, в инлайн режиме» - the game's windows still in the old look,
// the item's and the skill's cards first): the item card and the skill card in style A (core/info_card) - the game's
// description tables read, the language folder the game picks, the card's places, its words and colours
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/info_card.hpp"
#include "test.hpp"

using namespace uagfx;

namespace {

float measure(const std::string& s, float px, bool) {  // a letter 0.55 of its height wide
    return float(std::count_if(s.begin(), s.end(), [](char c) { return (uint8_t(c) & 0xC0) != 0x80; })) * px * 0.55f;
}

infocard::Texts texts() {
    infocard::Texts t;
    t.slots = "Слоти";
    t.empty = "порожньо";
    return t;
}

}  // namespace

// The tables: «1201#» then its lines then «#»; a skill's «MG_FIREWALL#» alike; «//» lines and the CR skipped
TEST(info_card_reads_the_description_tables) {
    const std::string items = "//file start\r\n501#\r\nA potion ^000088about 45 HP^000000.\r\n"
                              "Weight: ^7777777^000000\r\n#\r\n1201#\r\nA simple knife.\r\nWeight: ^77777740^000000\r\n"
                              "#\r\n";
    const auto d = infocard::descriptions(items);
    CHECK(d.size() == 2 && d.at("501").size() == 2 && d.at("1201").size() == 2);
    CHECK(d.at("501")[0] == "A potion ^000088about 45 HP^000000." && d.at("1201")[0] == "A simple knife.");
    const auto s = infocard::descriptions("NV_BASIC#\nBasic Skill\nMAX Lv : 9\n#\nMG_FIREWALL#\nFire Wall\n#\n");
    CHECK(s.size() == 2 && s.at("MG_FIREWALL").size() == 1 && s.at("NV_BASIC")[1] == "MAX Lv : 9");
    const auto slots = infocard::slotCounts("1101#3#\r\n1102#4#\r\nbad\r\n1103#0#\r\n");
    CHECK(slots.size() == 3 && slots.at(1101) == 3 && slots.at(1102) == 4 && slots.at(1103) == 0);
}

// The game's tables in the language's folder as uaRO.exe picks it by Windows' language (0x4EF2xx): German, Italian,
// Portuguese, Turkish their own, the rest English (Ukrainian and Russian among them)
TEST(info_card_language_folder) {
    CHECK(infocard::languageFolder(0x0407) == "german\\" && infocard::languageFolder(0x0410) == "italian\\");
    CHECK(infocard::languageFolder(0x0416) == "portuguese\\" && infocard::languageFolder(0x041F) == "turkish\\");
    CHECK(infocard::languageFolder(0x0422) == "english\\" && infocard::languageFolder(0x0419) == "english\\");
    CHECK(infocard::languageFolder(0x040C) == "english\\");  // French: English, as the game
}

// The card: the title over the icon's row, the close button at its right; the description wrapped under it - a long
// one shows its first rows, the wheel moves by three, kept within; a big picture narrows the words beside it; an
// item's slots under the words; kept on the screen
TEST(info_card_places) {
    infocard::Card c;
    c.title = "+7 Blade [4]";
    for (int i = 0; i < 60; ++i) c.lines.push_back("Line number " + std::to_string(i) + " of a long description");
    c.cards = {"Hydra Card", "", "", ""};
    infocard::Show sh;
    const infocard::Layout L = infocard::layout(c, 100, 100, 1920, 1080, 1.0f, sh, texts(), measure);
    CHECK(L.glass.w > 250 && L.glass.w < 420);
    CHECK(L.close.x > L.title.x && L.close.x + L.close.w <= L.glass.x + L.glass.w + 0.5f);
    CHECK(L.rows >= 60 && L.shownRows > 5 && L.shownRows < L.rows);
    CHECK(infocard::scrolled(L, 0, 1) == 3 && infocard::scrolled(L, 0, -1) == 0);
    CHECK(infocard::scrolled(L, 0, 100) == L.rows - L.shownRows);
    CHECK(L.slots.size() == 4 && L.slots[0].y > L.body.y);
    const float lx = float(L.x), ly = float(L.y);
    CHECK(infocard::hitTest(L, L.close.x + 4, L.close.y + 4) == infocard::Part::Close);
    CHECK(infocard::hitTest(L, L.body.x + 4, L.body.y + 4) == infocard::Part::Body);
    CHECK(infocard::hitTest(L, -50, -50) == infocard::Part::None);
    (void)lx;
    (void)ly;
    const infocard::Layout far = infocard::layout(c, 1900, 1060, 1920, 1080, 1.0f, sh, texts(), measure);
    CHECK(far.x + far.w <= 1920 && far.y + far.h <= 1080 && far.x >= 0 && far.y >= 0);
    infocard::Card p = c;
    panel::Picture big;
    big.width = 75;
    big.height = 100;
    big.px.assign(75 * 100, 0xFF808080u);
    p.picture = &big;
    const infocard::Layout withPicture = infocard::layout(p, 100, 100, 1920, 1080, 1.0f, sh, texts(), measure);
    CHECK(withPicture.picture.w >= 75 && withPicture.picture.h >= 100);
    float widest = 0, widestPlain = 0;
    for (const auto& r : withPicture.lines) widest = std::max(widest, r.box.x + r.box.w);
    for (const auto& r : L.lines) widestPlain = std::max(widestPlain, r.box.x + r.box.w);
    CHECK(widest <= withPicture.picture.x + 0.5f && widestPlain > withPicture.picture.x);
}

// Its words: the title bold, the note under it; the description's runs - black the card's own words, a dark colour
// lit to read on the glass, the game's white spacer (^ffffff_ on its white window) hidden; the slots' cards, an empty
// slot's words; the rows scrolled away not among them
TEST(info_card_words_and_colours) {
    infocard::Card c;
    c.title = "Fire Wall";
    c.note = "Lv 10";
    c.lines = {"Property: ^bb0000Fire^000000", "^ffffff_^000000", "Weight: ^77777740^000000"};
    c.cards = {"Hydra Card", ""};
    infocard::Show sh;
    const infocard::Layout L = infocard::layout(c, 100, 100, 1920, 1080, 1.0f, sh, texts(), measure);
    const std::vector<infocard::Words> w = infocard::words(L, c, sh, texts());
    const auto find = [&](const std::string& t) {
        return std::find_if(w.begin(), w.end(), [&](const infocard::Words& x) { return x.text == t; });
    };
    CHECK(find("Fire Wall") != w.end() && find("Fire Wall")->bold && find("Lv 10") != w.end());
    CHECK(find("Property: ") != w.end() && find("Fire") != w.end());
    if (find("Fire") != w.end() && find("Property: ") != w.end()) {
        CHECK(((find("Fire")->rgb >> 16) & 0xFF) > 0xC0);  // the red lit
        CHECK(find("Property: ")->rgb != 0x000000);        // black: the card's words
    }
    CHECK(find("_") == w.end());  // the white spacer
    CHECK(find("Hydra Card") != w.end() && find("порожньо") != w.end());
    infocard::Card longer = c;
    for (int i = 0; i < 60; ++i) longer.lines.push_back("Row " + std::to_string(i));
    infocard::Show scrolled;
    const infocard::Layout L2 = infocard::layout(longer, 100, 100, 1920, 1080, 1.0f, scrolled, texts(), measure);
    scrolled.scroll = infocard::scrolled(L2, 0, 100);
    const std::vector<infocard::Words> w2 = infocard::words(L2, longer, scrolled, texts());
    const bool first = std::any_of(w2.begin(), w2.end(), [](const infocard::Words& x) { return x.text == "Fire"; });
    const bool last = std::any_of(w2.begin(), w2.end(), [](const infocard::Words& x) { return x.text == "Row 59"; });
    CHECK(!first && last);
    panel::Picture out;
    out.width = L.w;
    out.height = L.h;
    out.px.assign(size_t(L.w) * size_t(L.h), 0u);
    infocard::paint(out, L, c, sh);
    CHECK((out.px[size_t(L.h / 2) * size_t(L.w) + size_t(L.w / 2)] >> 24) > 0x80);  // the card's glass
}

// A picture fitted into a room, its sides in proportion, premultiplied
TEST(info_card_picture_fitted) {
    std::vector<uint32_t> argb(75 * 100, 0xFF2040C0u);
    argb[0] = 0x00000000u;  // see-through
    const panel::Picture p = infocard::fitted(argb, 75, 100, 150, 150);
    CHECK(p.height == 150 && p.width == 113);  // 75 x 1.5 rounded
    CHECK((p.px[0] >> 24) < 0xFF && (p.px[size_t(75) * size_t(p.width) + 56] & 0xFF) > 0xA0);
    const panel::Picture none = infocard::fitted({}, 0, 0, 150, 150);
    CHECK(none.width == 0);
}
