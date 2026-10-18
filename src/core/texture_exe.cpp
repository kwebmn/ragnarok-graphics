#include "core/texture_exe.hpp"

#include <emmintrin.h>

#include <cstddef>
#include <cstring>

namespace uagfx::texture_exe {
namespace {

inline uint32_t a1r5g5b5(uint32_t c) {
    return ((c >> 6) & 0x3E0) | ((c >> 3) & 0x1F) | ((c >> 16) & 0x8000) | ((c >> 9) & 0x7C00);
}
inline uint32_t a4r4g4b4(uint32_t c) {
    return ((c >> 12) & 0xF00) | ((c >> 8) & 0xF0) | ((c >> 4) & 0xF) | ((c >> 16) & 0xF000);
}
inline uint32_t r5g6b5(uint32_t c) { return ((c >> 5) & 0x7E0) | ((c >> 3) & 0x1F) | ((c >> 8) & 0xF800); }

inline void put16(uint8_t* out, int i, uint32_t v) {
    const uint16_t w = uint16_t(v);
    std::memcpy(out + 2 * size_t(i), &w, 2);
}

// Four 32-bit values of 0-0xFFFF each, two sets, as eight 16-bit ones: signed packing of the values less 0x8000.
inline __m128i pack16(__m128i a, __m128i b) {
    const __m128i bias = _mm_set1_epi32(0x8000);
    const __m128i p = _mm_packs_epi32(_mm_sub_epi32(a, bias), _mm_sub_epi32(b, bias));
    return _mm_add_epi16(p, _mm_set1_epi16(short(0x8000)));
}

// Eight texels of A1R5G5B5 (bits as a1r5g5b5) or A4R4G4B4, magenta and (key) black made 0.
template <bool Four>
inline __m128i eight(const uint32_t* src, __m128i keyMask) {
    __m128i v[2];
    for (int h = 0; h < 2; ++h) {
        const __m128i c = _mm_loadu_si128(reinterpret_cast<const __m128i*>(src + 4 * h));
        __m128i x, rgb, magenta;
        if (Four) {
            x = _mm_or_si128(
                _mm_or_si128(_mm_and_si128(_mm_srli_epi32(c, 12), _mm_set1_epi32(0xF00)),
                             _mm_and_si128(_mm_srli_epi32(c, 8), _mm_set1_epi32(0xF0))),
                _mm_or_si128(_mm_and_si128(_mm_srli_epi32(c, 4), _mm_set1_epi32(0xF)),
                             _mm_and_si128(_mm_srli_epi32(c, 16), _mm_set1_epi32(0xF000))));
            rgb = _mm_and_si128(x, _mm_set1_epi32(0xFFF));
            magenta = _mm_cmpeq_epi32(rgb, _mm_set1_epi32(0xF0F));
        } else {
            x = _mm_or_si128(
                _mm_or_si128(_mm_and_si128(_mm_srli_epi32(c, 6), _mm_set1_epi32(0x3E0)),
                             _mm_and_si128(_mm_srli_epi32(c, 3), _mm_set1_epi32(0x1F))),
                _mm_or_si128(_mm_and_si128(_mm_srli_epi32(c, 16), _mm_set1_epi32(0x8000)),
                             _mm_and_si128(_mm_srli_epi32(c, 9), _mm_set1_epi32(0x7C00))));
            rgb = _mm_and_si128(x, _mm_set1_epi32(0x7FFF));
            magenta = _mm_cmpeq_epi32(x, _mm_set1_epi32(0xFC1F));
        }
        const __m128i black = _mm_and_si128(_mm_cmpeq_epi32(rgb, _mm_setzero_si128()), keyMask);
        v[h] = _mm_andnot_si128(_mm_or_si128(magenta, black), x);
    }
    return pack16(v[0], v[1]);
}

}  // namespace

void convertRow(uint32_t format, const uint32_t* src, int step, int count, bool key, void* dst) {
    uint8_t* out = static_cast<uint8_t*>(dst);
    int i = 0;
    const __m128i keyMask = key ? _mm_set1_epi32(-1) : _mm_setzero_si128();
    switch (format) {
        case 0:
            if (step == 1)
                for (; i + 8 <= count; i += 8)
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2 * size_t(i)), eight<false>(src + i, keyMask));
            for (; i < count; ++i) {
                const uint32_t v = a1r5g5b5(src[size_t(i) * size_t(step)]);
                put16(out, i, v == 0xFC1F || (key && (v & 0x7FFF) == 0) ? 0 : v);
            }
            break;
        case 1:
            if (step == 1)
                for (; i + 8 <= count; i += 8)
                    _mm_storeu_si128(reinterpret_cast<__m128i*>(out + 2 * size_t(i)), eight<true>(src + i, keyMask));
            for (; i < count; ++i) {
                const uint32_t v = a4r4g4b4(src[size_t(i) * size_t(step)]);
                const uint32_t rgb = v & 0xFFF;
                put16(out, i, rgb == 0xF0F || (key && rgb == 0) ? 0 : v);
            }
            break;
        case 2:
            for (; i < count; ++i) {
                const uint32_t v = r5g6b5(src[size_t(i) * size_t(step)]);
                if (!key || v != 0xF81F) put16(out, i, v);  // a keyed magenta texel keeps what it had
            }
            break;
        case 4:
            for (; i < count; ++i) {
                const uint32_t c = src[size_t(i) * size_t(step)];
                const uint32_t v = key && c == 0 ? 0 : c;
                std::memcpy(out + 4 * size_t(i), &v, 4);
            }
            break;
        default: break;
    }
}

}  // namespace uagfx::texture_exe
