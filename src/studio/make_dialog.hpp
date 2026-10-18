#pragma once
#include <windows.h>

#include <string>

#include "core/ai/chain.hpp"
#include "core/atlas_tiles.hpp"
#include "core/settings/strings.hpp"

namespace uagfx::studio {

// 2.3 (T4, T5, T6): Make again's choices - the model, the size, the general model's denoise, the chain's steps before
// and after the network, an atlas's tiles one by one (when the maps lay the texture as one). Modal; c comes in as the
// last choice and goes out as the new one; false: cancelled.
bool askChain(HWND owner, settings::Lang lang, HFONT font, int dpi, const std::wstring& what, atlas_tiles::Grid grid,
              ai::Chain& c);

}  // namespace uagfx::studio
