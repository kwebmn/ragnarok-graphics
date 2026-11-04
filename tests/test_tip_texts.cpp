// 3.0.11-beta.219 (the tester: «вікно tips ... треба декомунізувати ... а то зараз українська + російська»): the uaRO
// client's 68 tips of the day (its tipoftheday.txt, Russian) in the interface's language - the driver's own words for
// them, in its order; another client's list (another count or first tip) stays the game's
#include <string>

#include "core/tip_texts.hpp"
#include "test.hpp"

using namespace uagfx;

TEST(tip_texts_ours_for_the_uaro_list) {
    CHECK_EQ(tiptexts::kCount, 68);
    CHECK(tiptexts::isUaroList(68, "* /savechat : сохраняет чат в текстовом файле."));
    CHECK(!tiptexts::isUaroList(63, "* /savechat : saves chat as text file."));
    CHECK(!tiptexts::isUaroList(68, "->Vous pouvez choisir"));
    CHECK_EQ(tiptexts::tip(33, settings::Lang::Uk), std::string("/where — назва місцевості та ваші координати."));
    CHECK(tiptexts::tip(33, settings::Lang::Ru).find("/where") == 0);
    CHECK(tiptexts::tip(33, settings::Lang::En).find("/where") == 0);
    CHECK(tiptexts::tip(68, settings::Lang::Uk).empty());
    CHECK(tiptexts::tip(-1, settings::Lang::Uk).empty());
    for (int i = 0; i < tiptexts::kCount; ++i)
        CHECK(!tiptexts::tip(i, settings::Lang::Uk).empty() && !tiptexts::tip(i, settings::Lang::Ru).empty() &&
              !tiptexts::tip(i, settings::Lang::En).empty());
}
