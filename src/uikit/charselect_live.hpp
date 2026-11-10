#pragma once
#include <functional>
#include <string>
#include <vector>

#include <RmlUi/Core/DataModelHandle.h>
#include <RmlUi/Core/Types.h>

#include "core/char_select.hpp"
#include "uikit/layered.hpp"

namespace Rml {
class ElementDocument;
}

namespace uagfx::uikit::charpick {

// 3.0.18-beta.106 (the owner: «делай 1-3», the login screens): the character select live - its head (the title, the
// page «N / M» with ‹ › on the pages' view) and its four buttons (a flex row, each by what the chosen one allows) by
// RmlUi; the cards a layered body (uikit/layered): the three places or the carousel's five - their sizes, faces, bars
// and words at core charselect's places (a composed line-up), each card and the carousel's arrows pressable there.
struct ButtonV {
    Rml::String label;
    bool can = false, primary = false, hot = false;
};
struct Model {
    float base = 12, width = 0;
    Rml::String ground = "#00000000", title, page;
    bool pages = false;  // ‹ › in the head (the pages' view; the carousel's are at its sides)
    std::vector<ButtonV> buttons;
    layered::Layer layer;
};

const char* rml();
void bind(Rml::DataModelConstructor& c, Model& m);
using Source = std::function<std::string(const panel::Picture*)>;
void fill(Model& m, const uagfx::charselect::Layout& L, const std::vector<uagfx::charselect::Slot>& slots,
          const uagfx::charselect::Show& sh, const uagfx::charselect::Texts& t, const Source& src);
bool refit(Rml::ElementDocument* doc, uagfx::charselect::Layout& L);

}  // namespace uagfx::uikit::charpick
