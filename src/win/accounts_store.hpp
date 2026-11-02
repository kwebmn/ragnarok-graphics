#pragma once
#include "core/accounts.hpp"

namespace uagfx::win::accountsstore {

// 3.0.11-beta.175 (the login concept's shelf): this Windows user's shelf (core/accounts), one for all the game's
// folders and windows - %LOCALAPPDATA%\uaroGraphics\accounts.dat, its words DPAPI-protected (win/dpapi). Read at the
// first ask and again when another game window changed the file (looked at every 2 s); a file that cannot be read is
// kept aside as accounts.dat.bad and the shelf starts empty. On the game's thread.
accounts::Shelf& shelf();
bool save();  // false: not written (the shelf stays for this game)
// «Забути всі акаунти»: the shelf empty, its file gone
bool forget();

}  // namespace uagfx::win::accountsstore
