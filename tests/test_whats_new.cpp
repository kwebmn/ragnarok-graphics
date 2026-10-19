#include "core/settings/whats_new.hpp"
#include "core/settings/window_place.hpp"
#include "test.hpp"

using namespace uagfx::settings;

namespace {

const char* const kNotes =
    "# uaRO Graphics 2.5.27 — проверка\n"
    "\n"
    "- **Модель Game AI 2.0.** Её можно\n"
    "  выбрать для `любой` группы.\n"
    "- Вторая строка.\r\n"
    "\n"
    "# uaRO Graphics 2.5.26 — проверка\n"
    "\n"
    "- Другое.\n";

}  // namespace

TEST(whats_new_takes_one_version) {
    CHECK(notesOf(kNotes, "2.5.27") == "• Модель Game AI 2.0. Её можно выбрать для любой группы.\n• Вторая строка.");
    CHECK(notesOf(kNotes, "2.5.26") == "• Другое.");
}

TEST(whats_new_missing_version_is_empty) {
    CHECK(notesOf(kNotes, "2.5.2").empty());  // not 2.5.26 or 2.5.27
    CHECK(notesOf(kNotes, "2.6.0").empty());
    CHECK(notesOf("", "2.5.27").empty());
}

TEST(window_place_round_trip) {
    const WindowPlace p{-1200, 100, -100, 820, true};
    const auto back = parsePlace(placeToken(p));
    CHECK(back && back->left == -1200 && back->top == 100 && back->right == -100 && back->bottom == 820 &&
          back->maximized);
    CHECK(placeToken({10, 20, 1110, 740, false}) == "10 20 1110 740 0");
}

TEST(window_place_rejects_nonsense) {
    CHECK(!parsePlace("1 2 3"));
    CHECK(!parsePlace("a b c d e"));
    CHECK(!parsePlace("0 0 50 600 0"));      // too narrow
    CHECK(!parsePlace("0 0 900 30000 0"));   // too tall
    CHECK(!parsePlace("0 0 900 700 2"));
    CHECK(!parsePlace("0 0 900 700 0 7"));
    CHECK(parsePlace(" 0 0 900 700 1 "));
}

TEST(whats_new_keeps_numbered_steps) {
    const char* const notes =
        "# uaRO Graphics 2.6.0 — проверка\n"
        "- Проверьте:\n"
        "  1. Первое\n"
        "     продолжение.\n"
        "  2. Второе.\n";
    CHECK(notesOf(notes, "2.6.0") == "• Проверьте:\n   1. Первое продолжение.\n   2. Второе.");
}
