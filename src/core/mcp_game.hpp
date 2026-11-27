#pragma once
#include <functional>
#include <string>
#include <vector>

#include "core/char_block.hpp"
#include "core/config.hpp"
#include "core/chat_panel.hpp"
#include "core/item_state.hpp"
#include "core/json.hpp"
#include "core/settings/strings.hpp"

// 3.0.24-beta.34 (docs/superpowers/plans/2026-11-27-mcp-full-test.md, task 5): the MCP's game tool - what an action
// did, seen from outside: the character's numbers, the things, the chat's last lines (the server's answers among them)
namespace uagfx::mcpgame {

json::Value character(const charblock::Data& d);
// The bag's, the cart's, the storage's things; `nameOf` an item's words by its number ("" unknown)
json::Value items(const std::vector<itemstate::Item>& bag, const std::vector<itemstate::Item>& cart,
                  const std::vector<itemstate::Item>& storage, const std::function<std::string(int id)>& nameOf);
json::Value chat(const std::vector<chatpanel::Line>& lines);
// 3.0.24-beta.34 (task 6): every setting whose «section.name» starts with `prefix` ("" all) - its value now, its kind
// (check, slider, number, choice, text) and what it takes, its name in `lang`
json::Value settings(const Config& c, const std::string& prefix, settings::Lang lang);

}  // namespace uagfx::mcpgame
