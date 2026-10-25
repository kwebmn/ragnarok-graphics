#pragma once

// 3.0.5-beta.9 (R-VMPQKX: lroundf a fifth of a Radeon 610M laptop's game thread on «Середні» - the grass's steps grid
// went through it 32 thousand times a frame): std::lround and std::round without the library's call (the 32-bit
// build's lroundf is a function each time, mingw-w64's, not an instruction) - the same answers: halves away from zero.
// For magnitudes a long holds.
namespace uagfx {

inline long roundAway(float v) {
    const long whole = long(v);           // towards zero
    const float rest = v - float(whole);  // exact: a float's fraction is one
    return rest >= 0.5f ? whole + 1 : rest <= -0.5f ? whole - 1 : whole;
}

inline long roundAway(double v) {
    const long whole = long(v);
    const double rest = v - double(whole);
    return rest >= 0.5 ? whole + 1 : rest <= -0.5 ? whole - 1 : whole;
}

inline float roundAwayF(float v) { return float(roundAway(v)); }  // std::round's

// int(std::floor(v)) and int(std::ceil(v)) likewise (floorf the same x87 call in the 32-bit build, six a ground
// triangle a frame for the grass's cells) - for magnitudes an int holds
inline int floorInt(float v) {
    const int whole = int(v);  // towards zero
    return float(whole) > v ? whole - 1 : whole;
}

inline int ceilInt(float v) {
    const int whole = int(v);
    return float(whole) < v ? whole + 1 : whole;
}

}  // namespace uagfx
