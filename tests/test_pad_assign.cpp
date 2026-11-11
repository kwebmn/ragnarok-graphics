// 3.0.11-beta.91 (the concept «Контроллер в интерфейсе uaRO», stage 6): «Призначити» - X in a window lists the
// controller's buttons that use the skill bar's cells, each with what its cell holds; the chosen one gets the item
// (core/pad_assign)
#include <algorithm>
#include <string>
#include <vector>

#include "core/pad_assign.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(pad_assign_rows) {
    const std::vector<padassign::Row> rows = {{2, "F1", "Double Strafe"}, {3, "F2", ""}, {5, "F4", "Blitz Beat"}};
    const padassign::Layout L = padassign::layout(rows.size(), 960, 540, 1.0f);
    CHECK_EQ(L.rows.size(), size_t(3));
    CHECK(L.rows[1].y > L.rows[0].y && L.x + L.w / 2 == 960);
    CHECK_EQ(padassign::hitTest(L, L.rows[2].x + 5, L.rows[2].y + 5), 2);
    const std::vector<padassign::Words> w = padassign::words(L, rows, "Red Potion", "Призначити на кнопку", "Поставити",
                                                            "Скасувати", "порожньо", false);
    const auto has = [&](const std::string& text) {
        return std::any_of(w.begin(), w.end(), [&](const padassign::Words& x) { return x.text == text; });
    };
    CHECK(has("Double Strafe") && has("порожньо") && has("F4") && has("X") && has("RB") && has("Red Potion"));
    CHECK(has("Призначити на кнопку") && has("Поставити") && has("Скасувати"));
}
