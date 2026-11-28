#pragma once
#include <string>
#include <vector>

#include "core/settings/strings.hpp"

namespace uagfx::checksnow {

// 3.0.24-beta.3 (the concept https://uaro.kiev.ua/docs/settings-simple/, the owner: «сделай какойто пункт, в котором
// если мы чтото проверяем он плявлялся там, чтобы не искать его долго, после проверок уберать оттуда»; «3 да» - in
// the beta channel alone, «4 отправляет»): «Перевіряємо зараз» in Alt+O - what this beta asks to be checked, each a
// card with its steps and, when it has one, the switch it needs. The list is kept here by hand, beta by beta: a check
// leaves it once the owner has seen it work.
struct Check {
    const char* id;     // kept in [utility] checks_done once «Працює ✓» is pressed
    const char* since;  // the beta it came with
    const char* key;    // the switch it needs ("" none)
    const char* title[3];  // en, uk, ru
    const char* steps[3];
};
const std::vector<Check>& all();
// The checks not marked done (`done`: the ids, comma separated). 3.0.24-beta.50 (the audit 28.11, item 11: «Перевіряємо
// зараз 55»): of the newest `betas` betas among them alone (marked, the ones before come); 0 - of every beta
constexpr int kShownBetas = 2;
std::vector<const Check*> open(const std::string& done, int betas = kShownBetas);
// `done` with `id` added (once)
std::string markDone(const std::string& done, const std::string& id);
// Shown only by a beta build (the beta channel's: its version says «-beta»)
bool shown(const std::string& version);
const char* words(const char* const texts[3], settings::Lang l);
// The mark «Працює ✓» sends: the check's name, its id and the build
std::string workedNote(const Check& c, const std::string& version);
// «Є проблема — звіт»'s first words in the report
std::string problemNote(const Check& c, settings::Lang l);

}  // namespace uagfx::checksnow
