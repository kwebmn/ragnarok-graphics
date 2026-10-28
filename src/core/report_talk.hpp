#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/reports.hpp"

namespace uagfx::talk {

// 3.0.9-beta.5 (the user: «делай все» - the support canvas' screen 1): «Звернення» as a messenger. What its chat shows,
// in order: «Надіслано само…» for an automatic report, each message as a bubble (words or screenshots), a status it
// set as a thin line under it, and «Розробник прочитав · time» after the last message the developer saw that nobody
// answered yet (the site's dev_seen_at).
struct Item {
    enum Kind : uint8_t { AutoSent, Message, Status, DevRead };
    Kind kind = Message;
    size_t message = 0;  // Message, Status: the conversation's message
};
std::vector<Item> timeline(const reports::Conversation& c);

// The report's path over the conversation: Надіслано → Прочитано → (Потрібна інформація, while it is or was) →
// Виправляємо → Виправлено → Закрито; the steps before the status done, the status's current
struct Step {
    reports::Status status = reports::Status::New;
    bool done = false, current = false;
};
std::vector<Step> steps(const reports::Conversation& c);
bool closedTab(reports::Status s);  // «Закриті»: closed only («Виправлено» waits for the player's word)
// 3.0.10-beta.2: the shown conversation is behind the list's summary of it (an answer not seen, a newer last message,
// another status or version) - read it again
bool behind(const reports::Summary& now, const reports::Conversation& shown);
// 3.0.10-beta.2: the bar's words when the developer waits - their last words in one run (lines joined, spaces made one,
// a signature line «— …» at the end left out); "" when they wrote none
std::string waitsText(const reports::Conversation& c);

}  // namespace uagfx::talk
