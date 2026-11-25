#include "core/wine_setup.hpp"

#include "test.hpp"

using namespace uagfx;

// 3.0.23-beta.13 (R-F93RB7): the game's exe's own DLL overrides under Wine - ours first, Wine's after
TEST(wine_setup_overrides_the_games_ddraw_and_dinput8) {
    const std::wstring key = L"Software\\Wine\\AppDefaults\\uaRO.exe\\DllOverrides";
    CHECK(wine_setup::appDefaultsKey(L"C:\\uaRO-HD\\uaRO.exe") == key);
    CHECK(wine_setup::appDefaultsKey(L"uaRO.exe") == key);
    CHECK(wine_setup::appDefaultsKey(L"C:\\games\\") == L"");
    const auto& o = wine_setup::overrides();
    CHECK_EQ(o.size(), size_t(2));
    CHECK(std::wstring(o[0].dll) == L"ddraw" && std::wstring(o[0].order) == L"native,builtin");
    CHECK(std::wstring(o[1].dll) == L"dinput8");
}
