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

TEST(text_file_sizes) {
    // upscaled.grf's size on the Ready textures page: megabytes below a gigabyte, a decimal of gigabytes from one.
    CHECK(app::sizeText(0, Lang::En) == "0 MB");
    CHECK(app::sizeText(390ull << 20, Lang::Ru) == "390 МБ");
    CHECK(app::sizeText((390ull << 20) + (600ull << 10), Lang::En) == "391 MB");
    CHECK(app::sizeText(2254ull << 20, Lang::En) == "2.2 GB");
    CHECK(app::sizeText(2254ull << 20, Lang::Uk) == "2,2 ГБ");
    CHECK(app::sizeText(1024ull << 20, Lang::Ru) == "1,0 ГБ");
}
