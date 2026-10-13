// Alt+letter on the settings app's controls (stage 2 spec §6; 0.6.0 review M6).
#include "app/mnemonic.hpp"
#include "test.hpp"

using uagfx::app::takeMnemonic;
using uagfx::app::withMnemonic;

TEST(mnemonic_takes_the_first_free_letter_of_a_word) {
    std::wstring taken;
    takeMnemonic(L"&Применить", taken);  // the window's own buttons
    takeMnemonic(L"Применить и и&грать", taken);
    takeMnemonic(L"&Отменить", taken);
    takeMnemonic(L"За&крыть", taken);
    CHECK(taken == L"пгок");
    CHECK(withMnemonic(L"Размер интерфейса", taken) == L"&Размер интерфейса");
    CHECK(withMnemonic(L"Резкость", taken) == L"Р&езкость");  // Р is taken: the next free letter
    CHECK(withMnemonic(L"Масштаб рендера", taken) == L"&Масштаб рендера");
    CHECK(withMnemonic(L"Монитор", taken) == L"Мо&нитор");
    CHECK(taken == L"пгокремн");
}

TEST(mnemonic_letters_are_taken_without_regard_to_case) {
    std::wstring taken;
    takeMnemonic(L"&Apply", taken);
    CHECK(withMnemonic(L"apply all", taken) == L"a&pply all");
    std::wstring ukrainian = L"ї";
    CHECK(withMnemonic(L"Їжак", ukrainian) == L"Ї&жак");  // Ї is ї
}

TEST(mnemonic_doubles_an_ampersand_and_leaves_a_text_with_no_free_letter) {
    std::wstring taken;
    CHECK(withMnemonic(L"Q & A", taken) == L"&Q && A");
    CHECK(taken == L"q");
    takeMnemonic(L"Q && A", taken);  // no letter marked
    CHECK(taken == L"q");
    std::wstring all = L"ab";
    CHECK(withMnemonic(L"ab", all) == L"ab");
    CHECK(all == L"ab");
}
