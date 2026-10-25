#pragma once
#include <windows.h>

#include <functional>
#include <string>

#include "app/update_run.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::app {

// 2.10: the windows of the update, the rollback, the repair and the removal - what the .bat files' consoles were:
// small, over everything, in the player's language (settings::windowLang).

enum class Answer { Play, Update };
// The question before the login window when the game was started directly: "Update X is out"; 3.0.5-beta.1 `back`:
// the way back to the release X («Реліз» chosen over a beta, update::backToRelease).
Answer askUpdate(const std::string& version, settings::Lang l, bool back);

// `work` on a thread of its own under a window with a bar - its steps shown, Cancel while it downloads or waits for
// the game (`report` then answers false). `owner` (the settings app) is disabled meanwhile. true when it ended well,
// else `error`: the work's, or "stopped".
using Work = std::function<bool(const Report& report, std::wstring& error)>;
bool runWithProgress(HWND owner, const std::wstring& title, settings::Lang l, bool thenGame, const Work& work,
                     std::wstring& error);

// "Remove uaRO Graphics from <dir>?" with "Keep the settings and the texture pack" (ticked); false: not now.
bool askUninstall(const std::wstring& dir, settings::Lang l, bool& keep);

void tell(const std::wstring& text, bool error);  // a message over everything

}  // namespace uagfx::app
