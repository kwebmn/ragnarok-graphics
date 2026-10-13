#pragma once

namespace uagfx {

// Stage 6, crisp text (docs/superpowers/specs/2026-10-13-stage6-crisp-text-design.md §3): the twin of a game window
// picture at scale K. Game pixel g covers twin pixels [twinEdge(g, K), twinEdge(g + 1, K)): side by side, each block
// at least one pixel when K >= 1. A picture of w x h has a twin of twinEdge(w, K) x twinEdge(h, K).
int twinEdge(int g, double K);
// ceil(x k): the first target pixel a quad from game coordinate x covers at k (stage 3, scaleRect).
int ceilEdge(int x, double k);

// The twin pixels [first, end) the texture of tile [t, t + n) holds on one axis: every twin pixel its quad shows
// (spec §3), cut at the twin's size.
struct TwinSpan {
    int first = 0;
    int end = 0;
};
TwinSpan tileSpan(int t, int n, int twinSize, double K);

// A window at game x0 whose tile starts at t: target pixel p shows texel p - tileOrigin() of the tile's texture.
int tileOrigin(int x0, int t, double k, double K);

// A twin made at K is shown only while the game is drawn at that scale, and only above 1.
bool twinMatches(double K, double k);

// Tile `index` of a window picture of width x height: the game cuts its pictures into tiles of 256 x 256 at most,
// row by row (docs/research/text.md §4.1). Past the last tile: an empty place.
struct TilePlace {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};
TilePlace tilePlace(int index, int width, int height);

}  // namespace uagfx
