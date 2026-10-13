#pragma once

namespace uagfx {

// Contrast adaptive sharpening (after AMD FidelityFX CAS, the 5-tap cross) of one colour channel of one pixel: c the
// pixel, n s w e its neighbours, values 0..1; sharpness 0..1 (CAS's own range: 0 still sharpens a little). Detail
// gets more local contrast; flat and linear areas stay as they are; the result never leaves the five pixels' range,
// so there are no halos. The present pixel shader's cas() does the same.
double casSharpen(double c, double n, double s, double w, double e, double sharpness);

}  // namespace uagfx
