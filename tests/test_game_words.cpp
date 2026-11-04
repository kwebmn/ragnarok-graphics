// 3.0.11-beta.218 (R-S8BERR): the game's Korean question on dismissing the mercenary shown in the player's language;
// the game's colour marks out of the message boxes' words
#include <string>

#include "core/game_words.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(game_words_mercenary_dismiss_translated) {
    // «^ff0000^ff0000용병을 삭제합니다.^000000^000000 삭제하실 경우 ...» as uaRO.exe has it (EUC-KR)
    const std::string ko =
        "^ff0000^ff0000\xBF\xEB\xBA\xB4\xC0\xBB \xBB\xE8\xC1\xA6\xC7\xD5\xB4\xCF\xB4\xD9.^000000^000000 "
        "\xBB\xE8\xC1\xA6\xC7\xCF\xBD\xC7 \xB0\xE6\xBF\xEC \xC1\xF6\xB1\xDD\xB1\xEE\xC1\xF6 \xC5\xB0\xBF\xEE "
        "\xB3\xBB\xBF\xAA\xC0\xCC \xB8\xF0\xB5\xCE \xBB\xE8\xC1\xA6\xB5\xCB\xB4\xCF\xB4\xD9. "
        "\xB0\xE8\xBC\xD3\xC7\xCF\xBD\xC3\xB0\xDA\xBD\xC0\xB4\xCF\xB1\xEE?";
    CHECK_EQ(gamewords::known(ko, settings::Lang::Uk),
             std::string("Звільнити найманця? Усе, чого він набув досі, буде втрачено."));
    CHECK(gamewords::known(ko, settings::Lang::En).find("Dismiss the mercenary") == 0);
    CHECK(gamewords::known("Are you sure?", settings::Lang::Uk).empty());
}

TEST(game_words_colour_marks_out) {
    CHECK_EQ(gamewords::withoutColours("^ff0000Warning^000000 text"), std::string("Warning text"));
    CHECK_EQ(gamewords::withoutColours("a ^12 b ^zzzzzz"), std::string("a ^12 b ^zzzzzz"));
    CHECK_EQ(gamewords::withoutColours("end ^ff00"), std::string("end ^ff00"));
}
