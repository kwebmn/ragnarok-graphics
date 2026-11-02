#pragma once
#include <cctype>
#include <cstdint>
#include <optional>
#include <string>

namespace uagfx::loginview {

// 3.0.11-beta.177 (the login concept; the owner chose to have all three): the login screen's views - «Класичний» (the
// card, ▾ the saved logins), «Полиця» (the shelf of accounts, the default), «Продовжити» (the last account's
// character, «Грати»); [interface] login_view, the login screen's «Вигляд». The shelf empty: the card whichever.
enum class View : uint8_t { Classic, Shelf, Continue };

inline const char* token(View v) {
    switch (v) {
        case View::Classic: return "classic";
        case View::Continue: return "continue";
        default: return "shelf";
    }
}

inline std::optional<View> parse(const std::string& text) {
    std::string t;
    for (const char c : text) t += char(std::tolower((unsigned char)c));
    for (const View v : {View::Classic, View::Shelf, View::Continue})
        if (t == token(v)) return v;
    return std::nullopt;
}

}  // namespace uagfx::loginview
