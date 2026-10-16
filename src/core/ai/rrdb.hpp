#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/ai/srvgg.hpp"

namespace uagfx::ai {

// 1.5: Real-ESRGAN's RRDBNet (RealESRGAN_x4plus_anime_6B, github.com/xinntao/Real-ESRGAN, BSD-3-Clause): a 3x3
// convolution from RGB to F features; B blocks of three dense blocks, each five convolutions that see the block's
// input and every output before them (G channels each, leaky ReLU 0.2), the fifth back to F, times 0.2 plus the
// input; each block times 0.2 plus its input; a convolution plus the first features; two steps of nearest x2 and a
// convolution (leaky ReLU); one more, and one to RGB. No input added at the end: the network draws all of it.
//
// It runs as a list of steps (Op), one 3x3 convolution each, over buffers of planes the GPU keeps in one scratch
// buffer. The scales the network multiplies by are folded into its weights when it is read (rrdbProgram), so a step
// is a convolution, an optional leaky ReLU and up to two inputs added.
struct RrdbModel {
    int features = 64;  // F
    int growth = 32;    // G
    int blocks = 6;     // B
    Model net;          // its convolutions (bias and weights into net.data, the scales folded in) and, as the
                        // PReLU slopes of the shared convolution code, F slopes of 0.2 (net.prelu[0])
};

// The buffers a program works in: the tile's picture, the first features, three of F for the blocks' passing on,
// the dense block's growing outputs, and the larger sizes after the enlargement.
enum class Buf : uint8_t { Img, First, P, Q, R, Grow, Up1, Up2, Up3, Out, Count };
int bufChannels(const RrdbModel& m, Buf b);
int bufScale(Buf b);  // 1, 2 or 4 times the tile's size

struct Op {
    int conv = 0;             // into net.convs
    Buf inA = Buf::Img;       // the first `split` input channels
    Buf inB = Buf::Count;     // the rest (the dense block's outputs), from channel 0 of it
    int split = 0;
    int inOffset = 0;         // channel of inA its input starts at
    Buf out = Buf::First;
    int outOffset = 0;        // channel of out it writes from
    bool lrelu = false;
    Buf res1 = Buf::Count;    // added after it, times res1Scale; Count = none
    bool res1Fifth = false;   // res1 times 0.2 (a block's end), else times 1
    Buf res2 = Buf::Count;    // added after it, times 1
    bool upsample = false;    // its input is half its size, read nearest
    int scale = 1;            // its output's size, times the tile's
};

// "RRDB" file (tools/ai_rrdb.py): header, then the convolutions in the network's order, bias then weights each.
bool parseRrdb(const std::vector<uint8_t>& file, RrdbModel& m, std::string& error);
// The steps, in order.
std::vector<Op> rrdbProgram(const RrdbModel& m);

// The program on the CPU for one tile (3 planes of width x height, 0..1): out 3 planes 4 times as large, clamped.
// border: past the tile's edge, at every size - the GPU's reference (one tile, no margin).
void runRrdb(const RrdbModel& m, const float* rgb, int width, int height, float* out, Border border);

// A picture larger than a tile goes through in tiles of kRrdbTile texels with kRrdbMargin of the picture around
// them (the network sees past a tile's edge; only its middle is kept). One that fits whole goes as one tile with
// no margin, its own edges as the picture's border says.
constexpr int kRrdbTile = 96;
constexpr int kRrdbMargin = 10;
struct Tile {
    int x = 0, y = 0, width = 0, height = 0;  // the part of the picture it makes
    int left = 0, top = 0;                     // margins taken around it (the input is larger by them)
    int inWidth = 0, inHeight = 0;             // the input's size
};
std::vector<Tile> rrdbTiles(int width, int height);
// A tile's input: `channels` planes of the picture (planes of width x height) around it, what lies past the
// picture's edge as `border` says.
void tileInput(const float* planes, int channels, int width, int height, const Tile& t, Border border,
               std::vector<float>& out);

// The GPU's side (ai_shaders.cpp): where each buffer's planes begin in the scratch buffer (words) for a tile's input
// of width x height - the last step's three planes where Up2's were, done with by then - and the sixteen constants
// of ai_conv for rows [row, end) of a step, and of ai_place for a tile (the image's level 0: pitch and start, words).
struct RrdbLayout {
    uint32_t at[int(Buf::Count)] = {};
    uint64_t words = 0;
};
RrdbLayout rrdbLayout(const RrdbModel& m, int width, int height);
void convConstants(const RrdbModel& m, const Op& o, const Tile& t, const RrdbLayout& l, uint32_t border, int row,
                   int end, uint32_t out[16]);
void placeConstants(const Tile& t, const RrdbLayout& l, uint32_t pitch, uint32_t at, int phase, uint32_t out[16]);
// ai_conv and ai_place as the GPU runs them, word for word (u: the scratch buffer, image: ARGB words) - the tests'
// and the GPU self-test's reference for each step.
void convReference(const uint32_t* k, const float* weights, std::vector<float>& u);
void placeReference(const uint32_t* k, const std::vector<float>& u, std::vector<uint32_t>& image);

}  // namespace uagfx::ai
