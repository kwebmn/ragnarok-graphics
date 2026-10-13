#pragma once
#include <cstdint>
#include <vector>

namespace uagfx {

// A pre-transformed vertex as the client submits it (FVF_TL1 uses u0/v0, FVF_TL2 also u1/v1).
struct TlVertex {
    float x, y, z, rhw;
    uint32_t diffuse, specular;  // ARGB
    float u0, v0, u1, v1;
};

// Direct3D 7 puts pixel centres on integer coordinates, Direct3D 10+ on half-integers: a D3D7 x becomes
// x + 0.5 in D3D12 pixel space. Returns the normalized device coordinates on a width x height target.
void tlToNdc(float x, float y, int width, int height, float& ndcX, float& ndcY);

enum class PixelCenters { D3D7, D3D10 };  // D3D10: what an uncorrected translation does (negative tests)

struct RefTexture {
    int width = 0;
    int height = 0;
    std::vector<uint32_t> argb;  // row-major
};

struct Rgb {
    double r, g, b;
};

// A small CPU rasterizer with Direct3D 7 rules, for tests: top-left fill, perspective-correct attributes via rhw,
// colour = stage-0 texture (bilinear or point, clamp or wrap) x diffuse. Double precision throughout.
class RefRaster {
public:
    RefRaster(int width, int height, PixelCenters centers);
    void clear(const Rgb& c);
    void strip(const TlVertex* v, int count, const RefTexture& tex, bool linear, uint32_t address);
    void triangle(const TlVertex& a, const TlVertex& b, const TlVertex& c, const RefTexture& tex, bool linear,
                  uint32_t address);
    Rgb at(int x, int y) const { return px_[size_t(y) * size_t(w_) + size_t(x)]; }
    int width() const { return w_; }
    int height() const { return h_; }

private:
    int w_;
    int h_;
    PixelCenters centers_;
    std::vector<Rgb> px_;
};

}  // namespace uagfx
