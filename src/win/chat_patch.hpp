#pragma once

namespace uagfx::win {

// 2.6.12: uaRO.exe's check of its sent chat lines' count made to hold (core/chat_exe.hpp): Enter in the chat box
// while a map loads no longer takes the game down. false, with a log line, when the exe is another build.
bool installChatFix();

}  // namespace uagfx::win
