#pragma once
#include "core/window_panel.hpp"

namespace uagfx::win::classpictures {

// 3.0.11-beta.178 (the login concept's cards): a job's portrait (core/class_art) from ddraw.dll's resources, decoded
// once (stb's JPEG); null when the job has none or it could not be read. On the game's thread.
const panel::Picture* forJob(int jobId);
// The character's head (win/head_picture) `side` pixels square, made once a look - the cards' picture where the job has
// no portrait (the novices, the first classes); null when the game's sprites give none
const panel::Picture* head(int sex, int hair, int colour, int side);

}  // namespace uagfx::win::classpictures
