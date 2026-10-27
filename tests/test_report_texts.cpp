// 3.0.9-beta.4: what the app's strip and the game's card say of an answer (core/report_texts)
#include <string>

#include "core/report_texts.hpp"
#include "test.hpp"

using namespace uagfx;
using settings::Lang;

TEST(report_texts_say_what_to_do) {
    reports::Answer a;
    a.code = "R-7BDUE3";
    a.status = reports::Status::NeedInfo;
    CHECK_EQ(reporttexts::strip(Lang::Uk, a),
             std::string("Є відповідь на звернення R-7BDUE3 · Потрібна інформація — розробник чекає на вашу "
                         "відповідь."));
    CHECK_EQ(reporttexts::hint(Lang::Uk, a), std::string("Розробник чекає на вашу відповідь."));
    a.status = reports::Status::Fixed;
    a.fixedIn = "3.0.9-beta.4";
    CHECK_EQ(reporttexts::strip(Lang::Uk, a),
             std::string("Є відповідь на звернення R-7BDUE3 · Виправлено в 3.0.9-beta.4 — оновіться й перевірте."));
    CHECK_EQ(reporttexts::hint(Lang::Ru, a), std::string("Исправлено в 3.0.9-beta.4 — обновитесь и проверьте."));
    a.status = reports::Status::Fixing;
    CHECK_EQ(reporttexts::strip(Lang::En, a),
             std::string("An answer to the report R-7BDUE3 · Being fixed — a new answer from the developer."));
    CHECK_EQ(reporttexts::hint(Lang::Uk, a), std::string("Відкрийте, щоб прочитати відповідь."));
    CHECK_EQ(reporttexts::status(Lang::Uk, reports::Status::Closed, ""), std::string("Закрито"));
}

// 3.0.9-beta.5: the list's second line - the last message's start with its author, else what the status means
TEST(report_texts_tell_the_lists_second_line) {
    reports::Summary s;
    s.status = reports::Status::NeedInfo;
    s.lastAuthor = "dev";
    s.lastText = "Надішліть\nз Pause";
    CHECK_EQ(reporttexts::listLine(Lang::Uk, s), std::string("Розробник: Надішліть з Pause"));
    s.lastAuthor = "player";
    CHECK_EQ(reporttexts::listLine(Lang::Uk, s), std::string("Ви: Надішліть з Pause"));
    s.lastText.clear();
    CHECK_EQ(reporttexts::listLine(Lang::Uk, s), std::string("Розробник чекає на вашу відповідь."));
    s.status = reports::Status::New;
    CHECK_EQ(reporttexts::listLine(Lang::Uk, s), std::string("Чекає на розробника"));
    CHECK_EQ(reporttexts::status(Lang::Uk, reports::Status::New, ""), std::string("Надіслано"));  // the player's word
}
