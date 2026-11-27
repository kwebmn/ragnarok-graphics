#pragma once
#include <functional>
#include <set>
#include <string>

#include <RmlUi/Core.h>

#include "core/ui_theme.hpp"

// 3.0.18-beta.94: RmlUi on the host for the live windows' tests - a stand-in font (each letter 0.55 of its size wide,
// bold 0.6; lines 1.2), no drawing, the built-in theme (core/ui_theme_css) at «uaui:theme.rcss». One context of the
// target's size; a document made from markup with its model bound as win/uaui's Doc does (its name on the body).
namespace rmlhost {

Rml::Context* context(int w = 1920, int h = 1080);
// Loaded and shown at (x, y); its layout done. Null (and the log's error in `error`) when RmlUi refused it
Rml::ElementDocument* load(const std::string& rml, const std::function<void(Rml::DataModelConstructor&)>& bind,
                           float x = 0, float y = 0, std::string* error = nullptr);
void update();  // the models' changes into the layout (every model dirtied whole)
// 3.0.21-beta.8 (R-SPPSF3: «Налаштування» open - the card's fan loud): a frame as the game's - Update and Render,
// no model dirtied - and what it cost: geometries compiled, words made into letters, and geometries drawn at
// another place or cut than the frame before (what win/uaui meshes anew)
struct Work {
    int compiled = 0, words = 0, moved = 0, drawn = 0;
    int measured = 0;  // 3.0.21-beta.9: words measured - a layout of the document measures them again
};
Work frame();
// 3.0.21-beta.14: the textures' loads asked by RmlUi, by source; the sources whose loads fail (none by default)
void failTextures(const std::set<std::string>& sources);
int textureLoads(const std::string& source);
void close(Rml::ElementDocument* doc);
// 3.0.24-beta.33 (R-8BCJAH «белый интерфейс не читабелен»): the built-in theme served through a theme (core/ui_theme
// themedCss) for the documents loaded after it; nullptr: as built (dark)
void theme(const uagfx::uitheme::Theme* t);
const std::string& log();  // RmlUi's warnings and errors since the start

}  // namespace rmlhost
