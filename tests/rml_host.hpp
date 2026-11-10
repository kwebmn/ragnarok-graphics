#pragma once
#include <functional>
#include <string>

#include <RmlUi/Core.h>

// 3.0.18-beta.94: RmlUi on the host for the live windows' tests - a stand-in font (each letter 0.55 of its size wide,
// bold 0.6; lines 1.2), no drawing, the built-in theme (core/ui_theme_css) at «uaui:theme.rcss». One context of the
// target's size; a document made from markup with its model bound as win/uaui's Doc does (its name on the body).
namespace rmlhost {

Rml::Context* context(int w = 1920, int h = 1080);
// Loaded and shown at (x, y); its layout done. Null (and the log's error in `error`) when RmlUi refused it
Rml::ElementDocument* load(const std::string& rml, const std::function<void(Rml::DataModelConstructor&)>& bind,
                           float x = 0, float y = 0, std::string* error = nullptr);
void update();  // the models' changes into the layout (every model dirtied whole)
void close(Rml::ElementDocument* doc);
const std::string& log();  // RmlUi's warnings and errors since the start

}  // namespace rmlhost
