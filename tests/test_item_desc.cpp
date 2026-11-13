#include <string>
#include <vector>

#include "core/item_desc.hpp"
#include "test.hpp"

using namespace uagfx;

// 3.0.18-beta.253 (R-WP985Q, the owner on Autumn Red Tea's card: «какие-то символы непонятные в карточке и описание
// мне кажется можно было сделать лучше»): the game's descriptions (data\idnum2itemdesctable.txt) are cut at about 25
// letters a line, their facts «Key: ^777777value^000000» (a value over several lines), colour marks, a white «_» for a
// gap - read into facts and paragraphs

TEST(item_desc_a_tea_its_words_one_paragraph_and_its_weight_a_fact) {
    const std::vector<std::string> lines = {"A tea made with 3", "different herbs that", "give it a refreshingly",
                                            "strong scent, but also", "give it a bland flavor. INT + 2",
                                            "^FFFFFF ^000000", "Weight: ^77777750^000000"};
    const itemdesc::Read r = itemdesc::read(lines);
    CHECK_EQ(r.paragraphs.size(), size_t(1));
    CHECK_EQ(r.paragraphs[0], std::string("A tea made with 3 different herbs that give it a refreshingly strong "
                                          "scent, but also give it a bland flavor. INT + 2"));
    CHECK_EQ(r.facts.size(), size_t(1));
    CHECK_EQ(r.facts[0].key, std::string("Weight"));
    CHECK_EQ(r.facts[0].value, std::string("50"));
    CHECK(itemdesc::keyOf(r.facts[0].key) == itemdesc::Key::Weight);
}

TEST(item_desc_a_weapons_facts_a_value_over_three_lines) {
    const std::vector<std::string> lines = {"A heavy bludgeon", "that makes a handy", "weapon in a pinch.",
                                            "Weapon Class: ^777777Mace^000000",
                                            "Attack Strength: ^77777723^000000",
                                            "Weight: ^77777770^000000", "Weapon Level: ^7777771^000000",
                                            "Required Level: ^7777772^000000", "Jobs: ^777777Novice,",
                                            "Swordman, Acolyte,", "and Merchant Classes^000000"};
    const itemdesc::Read r = itemdesc::read(lines);
    CHECK_EQ(r.paragraphs.size(), size_t(1));
    CHECK_EQ(r.paragraphs[0], std::string("A heavy bludgeon that makes a handy weapon in a pinch."));
    CHECK_EQ(r.facts.size(), size_t(6));
    CHECK(itemdesc::keyOf(r.facts[0].key) == itemdesc::Key::Class && r.facts[0].value == "Mace");
    CHECK(itemdesc::keyOf(r.facts[1].key) == itemdesc::Key::Attack && r.facts[1].value == "23");
    CHECK(itemdesc::keyOf(r.facts[3].key) == itemdesc::Key::WeaponLevel && r.facts[3].value == "1");
    CHECK(itemdesc::keyOf(r.facts[4].key) == itemdesc::Key::RequiredLevel && r.facts[4].value == "2");
    CHECK(itemdesc::keyOf(r.facts[5].key) == itemdesc::Key::Jobs);
    CHECK_EQ(r.facts[5].value, std::string("Novice, Swordman, Acolyte, and Merchant Classes"));
}

TEST(item_desc_a_cards_effects_each_its_own_line) {
    const std::vector<std::string> lines = {"VIT +1", "Max HP +100", "Item Class: ^777777Card^000000",
                                            "Compound on: ^777777Weapon^000000", "Weight: ^7777771^000000"};
    const itemdesc::Read r = itemdesc::read(lines);
    CHECK_EQ(r.paragraphs.size(), size_t(2));
    CHECK_EQ(r.paragraphs[0], std::string("VIT +1"));
    CHECK_EQ(r.paragraphs[1], std::string("Max HP +100"));
    CHECK_EQ(r.facts.size(), size_t(3));
    CHECK(itemdesc::keyOf(r.facts[1].key) == itemdesc::Key::CompoundOn && r.facts[1].value == "Weapon");
}

TEST(item_desc_colour_marks_and_the_gap_line_left_out) {
    const std::vector<std::string> lines = {"A potion made from", "grinded Red Herbs that",
                                            "restores ^000088about 45 HP^000000.", "^ffffff_^000000",
                                            "Weight: ^7777777^000000"};
    const itemdesc::Read r = itemdesc::read(lines);
    CHECK_EQ(r.paragraphs.size(), size_t(1));
    CHECK_EQ(r.paragraphs[0], std::string("A potion made from grinded Red Herbs that restores about 45 HP."));
    CHECK_EQ(r.facts.size(), size_t(1));
}

TEST(item_desc_a_sentence_on_to_a_capital_after_a_word) {
    // «... against the» / «Curse status by 10%.»: a line ending in a word goes on; one ending in a figure does not
    const std::vector<std::string> lines = {"by his grandson. Increases resistance against the",
                                            "^880000Curse^000000 status by 10%.", "ATK +5", "DEF +3"};
    const itemdesc::Read r = itemdesc::read(lines);
    CHECK_EQ(r.paragraphs.size(), size_t(3));
    CHECK_EQ(r.paragraphs[0], std::string("by his grandson. Increases resistance against the Curse status by 10%."));
    CHECK_EQ(r.paragraphs[1], std::string("ATK +5"));
    CHECK_EQ(r.paragraphs[2], std::string("DEF +3"));
}

TEST(item_desc_known_keys_without_colour_and_others_kept) {
    const std::vector<std::string> lines = {"Weight : 10", "Applicable Pet: ^777777Poring^000000", "Note: read me"};
    const itemdesc::Read r = itemdesc::read(lines);
    CHECK_EQ(r.facts.size(), size_t(2));  // a known key without colour; a coloured value of a key not known
    CHECK(itemdesc::keyOf(r.facts[0].key) == itemdesc::Key::Weight && r.facts[0].value == "10");
    CHECK(itemdesc::keyOf(r.facts[1].key) == itemdesc::Key::Unknown && r.facts[1].value == "Poring");
    CHECK_EQ(r.paragraphs.size(), size_t(1));  // «Note: read me» - words, not a fact
    CHECK(itemdesc::keyOf("Required Lv.") == itemdesc::Key::RequiredLevel);
    CHECK(itemdesc::keyOf("Defense Rate") == itemdesc::Key::Defense);
    CHECK(itemdesc::keyOf("Head Position") == itemdesc::Key::Location);
    CHECK(itemdesc::keyOf("Property") == itemdesc::Key::Element);
    CHECK(itemdesc::read({}).facts.empty());
}

TEST(item_desc_a_sets_names_each_a_line_and_a_fact_after_a_colour_mark) {
    // a set's things one under the other (short lines, capitals) stay apart; «^000000Attack Strength : 199» a fact
    const std::vector<std::string> lines = {"When worn with Thorny Buckler:", "Bison Set", "Thorny Buckler",
                                            "Bison Horn", "^0000ffAttack Strength : 199^000000", "Rental Item",
                                            "The one-handed sword of legend."};
    const itemdesc::Read r = itemdesc::read(lines);
    CHECK_EQ(r.paragraphs.size(), size_t(6));
    CHECK_EQ(r.paragraphs[1], std::string("Bison Set"));
    CHECK_EQ(r.paragraphs[3], std::string("Bison Horn"));
    CHECK_EQ(r.paragraphs[4], std::string("Rental Item"));
    CHECK_EQ(r.facts.size(), size_t(1));
    CHECK(itemdesc::keyOf(r.facts[0].key) == itemdesc::Key::Attack && r.facts[0].value == "199");
}

TEST(item_desc_two_facts_on_one_line) {
    const std::vector<std::string> lines = {"Weapon Class : ^777777Sword^000000      Attack Strength : "
                                            "^777777199^000000",
                                            "Weight : ^7777770^000000     Property : ^777777Holy^000000"};
    const itemdesc::Read r = itemdesc::read(lines);
    CHECK_EQ(r.facts.size(), size_t(4));
    CHECK(r.paragraphs.empty());
    CHECK(itemdesc::keyOf(r.facts[1].key) == itemdesc::Key::Attack && r.facts[1].value == "199");
    CHECK(itemdesc::keyOf(r.facts[3].key) == itemdesc::Key::Element && r.facts[3].value == "Holy");
}
