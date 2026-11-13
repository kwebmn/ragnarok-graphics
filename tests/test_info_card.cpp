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
    // 3.0.18-beta.109 (the owner: «приближенно как было в оригинале»): the big picture on the left, the words right of
    // it, as the game's own item window
    float leftmost = 1e9f;
    for (const auto& r : withPicture.lines) leftmost = std::min(leftmost, r.box.x);
    CHECK(withPicture.picture.x < 40);
    CHECK(leftmost >= withPicture.picture.x + withPicture.picture.w - 0.5f);
    float widest = 0;
    for (const auto& r : withPicture.lines) widest = std::max(widest, r.box.x + r.box.w);
    CHECK(widest <= withPicture.glass.w);
}

// 3.0.18-beta.109: the slots a row of card pictures under the words - each its own place (a click opens that card)
TEST(info_card_slots_are_pressable) {
    infocard::Card c;
    c.title = "+7 Blade [4]";
    c.lines = {"A short blade."};
    c.cards = {"Hydra Card", "Skeleton Worker Card", "", ""};
    infocard::Show sh;
    const infocard::Layout L = infocard::layout(c, 100, 100, 1920, 1080, 1.0f, sh, texts(), measure);
    CHECK_EQ(L.slots.size(), size_t(4));
    CHECK(L.slots[0].h >= 30);  // room for a card's picture
    CHECK_EQ(infocard::slotAt(L, L.slots[1].x + 4, L.slots[1].y + 4), 1);
    CHECK_EQ(infocard::slotAt(L, L.body.x + 2, L.body.y + 2), -1);
    CHECK(L.slots[3].x > L.slots[0].x);  // a row
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
    }
    ;
    CHECK(find("Fire Wall") != w.end() && find("Fire Wall")->bold && find("Lv 10") != w.end());
    CHECK(find("Property: ") != w.end() && find("Fire") != w.end());
    if (find("Fire") != w.end() && find("Property: ") != w.end()) {
        CHECK(((find("Fire")->rgb >> 16) & 0xFF) > 0xC0);  // the red lit
        CHECK(find("Property: ")->rgb != 0x000000);        // black: the card's words
    }
    CHECK(find("_") == w.end());
    // the white spacer

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
    // the card's glass

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

// 3.0.11-beta.200 (the tester: «скіл вивчено, а замість його опису пише що не вивчено» - Coin Flip 5/5): the game's
// skill card keeps the skill's name as shown (FUN_00505d60's, «Coin Flip»: the table's «Coin_Flip» spaced), not its
// AEGIS name - the card's skill is the session's one of that name (its AEGIS name, its level), else the table's
TEST(info_card_skill_by_its_shown_name) {
    const std::vector<infocard::LearnedSkill> session = {
        {"GS_GLITTERING", 5}, {"GS_FLING", 0}, {"GS_TRACKING", 0}, {"GS_SINGLEACTION", 10}};
    const std::map<std::string, std::string> names = {{"GS_GLITTERING", "Coin_Flip"}, {"GS_FLING", "Coin_Fling"},
                                                      {"GS_TRACKING", "Tracking"}, {"GS_SINGLEACTION", "Single_Action"},
                                                      {"AL_HEAL", "Heal"}};
    infocard::LearnedSkill s = infocard::skillOfCard("Coin Flip", session, names);
    CHECK(s.aegis == "GS_GLITTERING" && s.level == 5);
    s = infocard::skillOfCard("Tracking", session, names);
    CHECK(s.aegis == "GS_TRACKING" && s.level == 0);
    s = infocard::skillOfCard("GS_SINGLEACTION", session, names);  // an AEGIS name, as before
    CHECK(s.aegis == "GS_SINGLEACTION" && s.level == 10);
    s = infocard::skillOfCard("Heal", session, names);  // not the character's: the table's, not learned
    CHECK(s.aegis == "AL_HEAL" && s.level == 0);
    s = infocard::skillOfCard("Nothing Such", session, names);
    CHECK(s.aegis == "Nothing Such" && s.level == 0);
}

// 3.0.11-beta.270 (the tester: Cure's «4Skill Requirements : Heal 2 / 8Skill Requirements ...»): a class's own lines
TEST(info_card_lines_for_the_job) {
    const std::vector<std::string> cure = {"MAX Lv : 1", "4^777777Skill Requirements : Heal 2^000000",
                                           "8^777777Skill Requirements : Heal 2^000000",
                                           "14^777777Skill Requirements : Faith 5^000000", "Target: ^777777Player"};
    const auto name = [](int job) { return job == 4 ? std::string("Acolyte") : job == 14 ? "Crusader" : ""; };
    const std::vector<std::string> crusader = infocard::forJob(cure, 14, name);
    CHECK(crusader.size() == 3 && crusader[1] == "^777777Skill Requirements : Faith 5^000000");
    const std::vector<std::string> mage = infocard::forJob(cure, 2, name);  // none of its own: each named
    CHECK(mage.size() == 5 && mage[1] == "Acolyte: ^777777Skill Requirements : Heal 2^000000");
    CHECK(mage[2] == "#8: ^777777Skill Requirements : Heal 2^000000");
    CHECK(infocard::forJob({"2^ff0000", "12 monsters"}, 2, name).size() == 2);  // «12 monsters»: no colour after it
}

// 3.0.18-beta.110 (the owner: «окно карты как в оригинале»): a card opened from a slot - its illustration big over its
// words, in the middle; the words under it the card's whole width
TEST(info_card_a_cards_art_over_its_words) {
    infocard::Card c;
    c.title = "Hydra Card";
    c.lines = {"Damage against Demi-Human monsters +20%."};
    c.art = true;
    panel::Picture art;
    int rw = 0, rh = 0;
    infocard::artRoom(1.0f, rw, rh);
    CHECK(rw >= 150 && rh >= 200);
    art.width = rw;
    art.height = rh;
    art.px.assign(size_t(rw) * size_t(rh), 0xFF808080u);
    c.picture = &art;
    infocard::Show sh;
    const infocard::Layout L = infocard::layout(c, 100, 100, 1920, 1080, 1.0f, sh, texts(), measure);
    CHECK(std::abs(L.picture.x + L.picture.w / 2 - L.glass.w / 2) < 1.5f);  // in the middle
    CHECK(!L.lines.empty() && L.lines.front().box.y >= L.picture.y + L.picture.h);  // the words under it
    CHECK(L.lines.front().box.x < L.picture.x);
}

// 3.0.18-beta.242 (the reference concept: a card opens from the item's own card «У довіднику»): a link under its words
// and slots - pressed it is Part::Reference, its words gold in its box; none when its words are not given
TEST(info_card_reference_link) {
    infocard::Card c;
    c.title = "Fly Wing";
    c.lines = {"A wing.", "Teleports you."};
    c.cards = {"", ""};
    infocard::Show sh;
    infocard::Texts t = texts();
    const infocard::Layout none = infocard::layout(c, 100, 100, 1920, 1080, 1.0f, sh, t, measure);
    CHECK_EQ(none.reference.w, 0.0f);
    t.reference = "У довіднику ›";
    const infocard::Layout L = infocard::layout(c, 100, 100, 1920, 1080, 1.0f, sh, t, measure);
    CHECK(L.reference.w > 0 && L.reference.y >= L.slots.back().y + L.slots.back().h);
    CHECK(L.reference.y + L.reference.h <= L.glass.y + L.glass.h && L.h > none.h);
    CHECK(infocard::hitTest(L, L.reference.x + 4, L.reference.y + L.reference.h / 2) == infocard::Part::Reference);
    bool said = false;
    for (const infocard::Words& w : infocard::words(L, c, sh, t))
        said = said || (w.text == "У довіднику ›" && w.box.x == L.reference.x && w.box.y == L.reference.y);
    CHECK(said);
}
