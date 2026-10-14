#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace uagfx::ai {

// Real-ESRGAN's compact network (SRVGGNetCompact, github.com/xinntao/Real-ESRGAN, BSD-3-Clause): a 3x3 convolution
// from RGB to F features and PReLU, N more of them from F to F, a last one to 3 * s * s channels, a pixel shuffle by
// s, and the input enlarged by s (nearest) added - the network learns only what the enlargement misses. Weights come
// from tools/ai_model.py ("SRVG" file): this is the reference the GPU's version is checked against, and the one the
// tests run.
struct Model {
    int inChannels = 3;
    int features = 64;
    int middle = 32;  // the F-to-F convolutions
    int scale = 4;
    struct Conv {
        int in = 0, out = 0;
        size_t bias = 0, weight = 0;  // into data: out floats; out * in * 9, [out][in][3][3]
    };
    std::vector<Conv> convs;     // middle + 2
    std::vector<size_t> prelu;   // middle + 1, features slopes each
    std::vector<float> data;
};

// false with error when the file is not a whole SRVG model.
bool parseModel(const std::vector<uint8_t>& file, Model& m, std::string& error);

// One 3x3 convolution with a zero border, output channels [first, last) only (callers may split them over threads).
// in: inC planes of width * height; out: outC planes.
void convolve(const Model& m, const Model::Conv& c, const float* in, int width, int height, float* out, int first,
              int last);
void prelu(const float* slopes, float* planes, int channels, int width, int height);

// The whole network on planar RGB (3 planes of width * height, 0..1): out gets 3 planes of (width * scale) x
// (height * scale), clamped to 0..1.
void upscale(const Model& m, const float* rgb, int width, int height, float* out);

// The last steps, shared by upscale and the GPU's checks: pixel shuffle of 3 * s * s planes into 3 planes s times as
// large, plus the input enlarged (nearest), clamped.
void shuffleAdd(const float* planes, const float* rgb, int width, int height, int scale, float* out);

}  // namespace uagfx::ai
