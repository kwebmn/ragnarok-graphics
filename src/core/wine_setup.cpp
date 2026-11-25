#include "core/wine_setup.hpp"

namespace uagfx::wine_setup {

const std::vector<Override>& overrides() {
    static const std::vector<Override> list = {{L"ddraw", L"native,builtin"}, {L"dinput8", L"native,builtin"}};
    return list;
}

std::wstring appDefaultsKey(const std::wstring& exe) {
    const size_t slash = exe.find_last_of(L"\\/");
    const std::wstring name = slash == std::wstring::npos ? exe : exe.substr(slash + 1);
    if (name.empty()) return L"";
    return L"Software\\Wine\\AppDefaults\\" + name + L"\\DllOverrides";
}

}  // namespace uagfx::wine_setup
