#pragma once
#include <string>

namespace uagfx::win::dpapi {

// 3.0.11-beta.175 (the login concept's shelf): this Windows user's own protection of a secret (crypt32's
// CryptProtectData, asked for only at the first use: the driver's imports stay the four) - another user of the
// computer, or a copy of the file on another one, cannot read it. False when Windows refuses.
bool protect(const std::string& plain, std::string& blob);
bool unprotect(const std::string& blob, std::string& plain);

}  // namespace uagfx::win::dpapi
