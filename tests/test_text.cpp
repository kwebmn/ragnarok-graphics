// The settings app's portable texts (src/app/text.cpp).
#include "app/text.hpp"
#include "test.hpp"

using namespace uagfx;
using settings::Lang;

TEST(app_text_scale_with_the_language_s_decimal_mark) {
    CHECK(app::scaleText(1.75, Lang::En) == "1.75");
    CHECK(app::scaleText(1.75, Lang::Ru) == "1,75");  // the spec writes "×1,75" (stage 2 spec §5.3)
    CHECK(app::scaleText(1.75, Lang::Uk) == "1,75");
    CHECK(app::scaleText(1.5, Lang::En) == "1.5");
    CHECK(app::scaleText(2.0, Lang::Ru) == "2");
}
